/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <functional>

#include "BLI_generic_pointer.hh"
#include "BLI_map.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "BKE_attribute_legacy_convert.hh"
#include "BKE_geometry_set.hh"

namespace blender::bke {

/**
 * Utility to group together multiple functions that are used to access custom data on geometry
 * components in a generic way.
 */
struct CustomDataAccessInfo {
  using CustomDataGetter = CustomData *(*)(void *owner);
  using ConstCustomDataGetter = const CustomData *(*)(const void *owner);
  using GetElementNum = int (*)(const void *owner);
  using GetTagModifiedFunction = std::function<void()> (*)(void *owner, StringRef name);

  CustomDataGetter get_custom_data;
  ConstCustomDataGetter get_const_custom_data;
  GetElementNum get_element_num;
  GetTagModifiedFunction get_tag_modified_function;
};

/**
 * A #BuiltinAttributeProvider is responsible for exactly one attribute on a geometry component.
 * The attribute is identified by its name and has a fixed domain and type. Builtin attributes do
 * not follow the same loose rules as other attributes, because they are mapped to internal
 * "legacy" data structures. For example, some builtin attributes cannot be deleted.
 */
class BuiltinAttributeProvider {
 public:
  enum DeletableEnum {
    Deletable,
    NonDeletable,
  };

 protected:
  const std::string name_;
  const AttrDomain domain_;
  const eCustomDataType data_type_;
  const DeletableEnum deletable_;
  const AttributeValidator validator_;
  const GPointer default_value_;

 public:
  BuiltinAttributeProvider(std::string name,
                           const AttrDomain domain,
                           const eCustomDataType data_type,
                           const DeletableEnum deletable,
                           AttributeValidator validator = {},
                           const GPointer default_value = {})
      : name_(std::move(name)),
        domain_(domain),
        data_type_(data_type),
        deletable_(deletable),
        validator_(validator),
        default_value_(default_value)
  {
  }

  virtual GAttributeReader try_get_for_read(const void *owner) const = 0;
  virtual GAttributeWriter try_get_for_write(void *owner) const = 0;
  virtual bool try_delete(void *owner) const = 0;
  virtual bool try_create(void *owner, const AttributeInit &initializer) const = 0;
  virtual bool exists(const void *owner) const = 0;

  StringRefNull name() const
  {
    return name_;
  }

  AttrDomain domain() const
  {
    return domain_;
  }

  eCustomDataType data_type() const
  {
    return data_type_;
  }

  AttributeValidator validator() const
  {
    return validator_;
  }

  GPointer default_value() const
  {
    return default_value_;
  }
};

/**
 * A #DynamicAttributesProvider manages a set of named attributes on a geometry component. Each
 * attribute has a name, domain and type.
 */
class DynamicAttributesProvider {
 public:
  virtual GAttributeReader try_get_for_read(const void *owner, StringRef attribute_id) const = 0;
  virtual GAttributeWriter try_get_for_write(void *owner, StringRef attribute_id) const = 0;
  virtual bool try_delete(void *owner, StringRef attribute_id) const = 0;
  virtual bool try_create(void *owner,
                          const StringRef attribute_id,
                          const AttrDomain domain,
                          const eCustomDataType data_type,
                          const AttributeInit &initializer) const
  {
    UNUSED_VARS(owner, attribute_id, domain, data_type, initializer);
    /* Some providers should not create new attributes. */
    return false;
  };

  /**
   * Return false when the iteration was stopped.
   */
  virtual bool foreach_attribute(const void *owner,
                                 FunctionRef<void(const AttributeIter &)> fn) const = 0;
  virtual void foreach_domain(const FunctionRef<void(AttrDomain)> callback) const = 0;
};

/**
 * This is the attribute provider for most user generated attributes.
 */
class CustomDataAttributeProvider final : public DynamicAttributesProvider {
 private:
  static constexpr uint64_t supported_types_mask = CD_MASK_PROP_ALL;
  AttrDomain domain_;
  CustomDataAccessInfo custom_data_access_;

 public:
  CustomDataAttributeProvider(const AttrDomain domain,
                              const CustomDataAccessInfo custom_data_access)
      : domain_(domain), custom_data_access_(custom_data_access)
  {
  }

  GAttributeReader try_get_for_read(const void *owner, StringRef attribute_id) const final;

  GAttributeWriter try_get_for_write(void *owner, StringRef attribute_id) const final;

  bool try_delete(void *owner, StringRef attribute_id) const final;

  bool try_create(void *owner,
                  StringRef attribute_id,
                  AttrDomain domain,
                  const eCustomDataType data_type,
                  const AttributeInit &initializer) const final;

  bool foreach_attribute(const void *owner,
                         FunctionRef<void(const AttributeIter &)> fn) const final;

  void foreach_domain(const FunctionRef<void(AttrDomain)> callback) const final
  {
    callback(domain_);
  }

 private:
  bool type_is_supported(eCustomDataType data_type) const
  {
    return ((1ULL << data_type) & supported_types_mask) != 0;
  }
};

/**
 * This provider is used to provide access to builtin attributes. It supports making internal types
 * available as different types.
 *
 * It also supports named builtin attributes, and will look up attributes in #CustomData by name
 * if the stored type is the same as the attribute type.
 */
class BuiltinCustomDataLayerProvider final : public BuiltinAttributeProvider {
  using AttrUpdateOnChange = void (*)(void *owner);
  const CustomDataAccessInfo custom_data_access_;
  const AttrUpdateOnChange update_on_change_;

 public:
  BuiltinCustomDataLayerProvider(std::string attribute_name,
                                 const AttrDomain domain,
                                 const eCustomDataType data_type,
                                 const DeletableEnum deletable,
                                 const CustomDataAccessInfo custom_data_access,
                                 const AttrUpdateOnChange update_on_change,
                                 const AttributeValidator validator = {},
                                 const GPointer default_value = {})
      : BuiltinAttributeProvider(
            std::move(attribute_name), domain, data_type, deletable, validator, default_value),
        custom_data_access_(custom_data_access),
        update_on_change_(update_on_change)
  {
  }

  GAttributeReader try_get_for_read(const void *owner) const final;
  GAttributeWriter try_get_for_write(void *owner) const final;
  bool try_delete(void *owner) const final;
  bool try_create(void *owner, const AttributeInit &initializer) const final;
  bool exists(const void *owner) const final;

 private:
  bool layer_exists(const CustomData &custom_data) const;
};

/**
 * This is a container for multiple attribute providers that are used by one geometry type
 * (e.g. there is a set of attribute providers for mesh components).
 */
class GeometryAttributeProviders {
 private:
  /**
   * Builtin attribute providers are identified by their name. Attribute names that are in this
   * map will only be accessed using builtin attribute providers. Therefore, these providers have
   * higher priority when an attribute name is looked up. Usually, that means that builtin
   * providers are checked before dynamic ones.
   */
  Map<std::string, const BuiltinAttributeProvider *> builtin_attribute_providers_;
  /**
   * An ordered list of dynamic attribute providers. The order is important because that is order
   * in which they are checked when an attribute is looked up.
   */
  Vector<const DynamicAttributesProvider *> dynamic_attribute_providers_;
  /**
   * All the domains that are supported by at least one of the providers above.
   */
  VectorSet<AttrDomain> supported_domains_;

 public:
  GeometryAttributeProviders(Span<const BuiltinAttributeProvider *> builtin_attribute_providers,
                             Span<const DynamicAttributesProvider *> dynamic_attribute_providers)
      : dynamic_attribute_providers_(dynamic_attribute_providers)
  {
    for (const BuiltinAttributeProvider *provider : builtin_attribute_providers) {
      /* Use #add_new to make sure that no two builtin attributes have the same name. */
      builtin_attribute_providers_.add_new(provider->name(), provider);
      supported_domains_.add(provider->domain());
    }
    for (const DynamicAttributesProvider *provider : dynamic_attribute_providers) {
      provider->foreach_domain([&](AttrDomain domain) { supported_domains_.add(domain); });
    }
  }

  const Map<std::string, const BuiltinAttributeProvider *> &builtin_attribute_providers() const
  {
    return builtin_attribute_providers_;
  }

  Span<const DynamicAttributesProvider *> dynamic_attribute_providers() const
  {
    return dynamic_attribute_providers_;
  }

  Span<AttrDomain> supported_domains() const
  {
    return supported_domains_;
  }
};

namespace attribute_accessor_functions {

template<const GeometryAttributeProviders &providers>
inline std::optional<AttributeDomainAndType> builtin_domain_and_type(const void * /*owner*/,
                                                                     const StringRef name)
{
  if (const BuiltinAttributeProvider *provider =
          providers.builtin_attribute_providers().lookup_default_as(name, nullptr))
  {
    const AttrType data_type = *custom_data_type_to_attr_type(provider->data_type());
    return AttributeDomainAndType{provider->domain(), data_type};
  }
  return std::nullopt;
}

template<const GeometryAttributeProviders &providers>
inline GPointer builtin_default_value(const void * /*owner*/, const StringRef attribute_id)
{
  if (const BuiltinAttributeProvider *provider =
          providers.builtin_attribute_providers().lookup_default_as(attribute_id, nullptr))
  {
    return provider->default_value();
  }
  return {};
}

template<const GeometryAttributeProviders &providers>
inline GAttributeReader lookup(const void *owner, const StringRef name)
{
  if (const BuiltinAttributeProvider *provider =
          providers.builtin_attribute_providers().lookup_default_as(name, nullptr))
  {
    return provider->try_get_for_read(owner);
  }
  for (const DynamicAttributesProvider *provider : providers.dynamic_attribute_providers()) {
    GAttributeReader attribute = provider->try_get_for_read(owner, name);
    if (attribute) {
      return attribute;
    }
  }
  return {};
}

template<const GeometryAttributeProviders &providers>
inline void foreach_attribute(const void *owner,
                              const FunctionRef<void(const AttributeIter &)> fn,
                              const AttributeAccessor &accessor)
{
  Set<StringRef, 16> handled_attribute_ids;
  for (const BuiltinAttributeProvider *provider : providers.builtin_attribute_providers().values())
  {
    if (provider->exists(owner)) {
      const auto get_fn = [&]() { return provider->try_get_for_read(owner); };
      AttributeIter iter{provider->name(),
                         provider->domain(),
                         *custom_data_type_to_attr_type(provider->data_type()),
                         get_fn};
      iter.is_builtin = true;
      iter.accessor = &accessor;
      fn(iter);
      if (iter.is_stopped()) {
        return;
      }
      handled_attribute_ids.add(iter.name);
    }
  }
  for (const DynamicAttributesProvider *provider : providers.dynamic_attribute_providers()) {
    const bool continue_loop = provider->foreach_attribute(owner, [&](const AttributeIter &iter) {
      if (handled_attribute_ids.add(iter.name)) {
        iter.accessor = &accessor;
        fn(iter);
      }
    });
    if (!continue_loop) {
      return;
    }
  }
}

template<const GeometryAttributeProviders &providers>
inline AttributeValidator lookup_validator(const void * /*owner*/, const blender::StringRef name)
{
  const BuiltinAttributeProvider *provider =
      providers.builtin_attribute_providers().lookup_default_as(name, nullptr);
  if (!provider) {
    return {};
  }
  return provider->validator();
}

template<const GeometryAttributeProviders &providers>
inline GAttributeWriter lookup_for_write(void *owner, const StringRef name)
{
  if (const BuiltinAttributeProvider *provider =
          providers.builtin_attribute_providers().lookup_default_as(name, nullptr))
  {
    return provider->try_get_for_write(owner);
  }
  for (const DynamicAttributesProvider *provider : providers.dynamic_attribute_providers()) {
    GAttributeWriter attribute = provider->try_get_for_write(owner, name);
    if (attribute) {
      return attribute;
    }
  }
  return {};
}

template<const GeometryAttributeProviders &providers>
inline bool remove(void *owner, const StringRef name)
{
  if (const BuiltinAttributeProvider *provider =
          providers.builtin_attribute_providers().lookup_default_as(name, nullptr))
  {
    return provider->try_delete(owner);
  }
  for (const DynamicAttributesProvider *provider : providers.dynamic_attribute_providers()) {
    if (provider->try_delete(owner, name)) {
      return true;
    }
  }
  return false;
}

template<const GeometryAttributeProviders &providers>
inline bool add(void *owner,
                const StringRef name,
                const AttrDomain domain,
                const AttrType data_type,
                const AttributeInit &initializer)
{
  const eCustomDataType custom_data_type = *attr_type_to_custom_data_type(data_type);
  if (const BuiltinAttributeProvider *provider =
          providers.builtin_attribute_providers().lookup_default_as(name, nullptr))
  {
    if (provider->domain() != domain) {
      return false;
    }
    if (provider->data_type() != custom_data_type) {
      return false;
    }
    return provider->try_create(owner, initializer);
  }
  for (const DynamicAttributesProvider *provider : providers.dynamic_attribute_providers()) {
    if (provider->try_create(owner, name, domain, custom_data_type, initializer)) {
      return true;
    }
  }
  return false;
}

template<const GeometryAttributeProviders &providers>
inline AttributeAccessorFunctions accessor_functions_for_providers()
{
  return AttributeAccessorFunctions{nullptr,
                                    nullptr,
                                    builtin_domain_and_type<providers>,
                                    builtin_default_value<providers>,
                                    lookup<providers>,
                                    nullptr,
                                    foreach_attribute<providers>,
                                    lookup_validator<providers>,
                                    lookup_for_write<providers>,
                                    remove<providers>,
                                    add<providers>};
}

}  // namespace attribute_accessor_functions

}  // namespace blender::bke

/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_create_info.hh"
#include "gpu_shader_private.hh"

#include "mtl_shader_interface.hh"

/** -- Metal Shader Generator for GLSL -> MSL conversion --
 *
 * The Metal shader generator class is used as a conversion utility for generating
 * a compatible MSL shader from a source GLSL shader. There are several steps
 * involved in creating a shader, and structural changes which enable the source
 * to function in the same way.
 *
 * 1) Extraction and conversion of shaders input's and output's to their Metal-compatible
 *    version. This is a subtle data transformation from GPUShaderCreateInfo, allowing
 *    for Metal-specific parameters.
 *
 * 2) Determine usage of shader features such as GL global variable usage, depth write output,
 *    clip distances, multilayered rendering, barycentric coordinates etc;
 *
 * 3) Generate MSL shader.
 *
 * 4) Populate #MTLShaderInterface, describing input/output structure, bind-points, buffer size and
 *    alignment, shader feature usage etc; Everything required by the Metal back-end to
 *    successfully enable use of shaders and GPU back-end features.
 *
 *
 *
 * For each shading stage, we generate an MSL shader following these steps:
 *
 * 1) Output custom shader defines describing modes e.g. whether we are using
 *    sampler bindings or argument buffers; at the top of the shader.
 *
 * 2) Inject common Metal headers.
 *    - `mtl_shader_defines.msl` is used to map GLSL functions to MSL.
 *    - `mtl_shader_common.msl` is added to ALL MSL shaders to provide
 *      common functionality required by the back-end. This primarily
 *      contains function-constant hooks, used in PSO generation.
 *
 * 3) Create a class Scope which wraps the GLSL shader. This is used to
 *    create a global per-thread scope around the shader source, to allow
 *    access to common shader members (GLSL globals, shader inputs/outputs etc)
 *
 * 4) Generate shader interface structs and populate local members where required for:
 *    - `VertexInputs`
 *    - `VertexOutputs`
 *    - `Uniforms`
 *    - `Uniform Blocks`
 *    - `textures` ;
 *    etc;
 *
 * 5) Inject GLSL source.
 *
 * 6) Generate MSL shader entry point function. Every Metal shader must have a
 *    vertex/fragment/kernel entry-point, which contains the function binding table.
 *    This is where bindings are specified and passed into the shader.
 *
 *    For converted shaders, the MSL entry-point will also instantiate a shader
 *    class per thread, and pass over bound resource references into the class.
 *
 *    Finally, the shaders "main()" method will be called, and outputs are copied.
 *
 *    NOTE: For position outputs, the default output position will be converted to
 *    the Metal coordinate space, which involves flipping the Y coordinate and
 *    re-mapping the depth range between 0 and 1, as with Vulkan.
 *
 *
 * The final shader structure looks as follows:
 *
 * \code{.cc}
 * -- Shader defines --
 * #define USE_ARGUMENT_BUFFER_FOR_SAMPLERS 0
 * ... etc ...;
 *
 * class MetalShaderVertexImp {
 *
 *  -- Common shader interface structs --
 *  struct VertexIn {
 *    vec4 pos [[attribute(0)]]
 *  }
 *  struct VertexOut {...}
 *  struct PushConstantBlock {...}
 *  struct drw_Globals {...}
 *  ...
 *
 *   -- GLSL source code --
 *   ...
 * };
 *
 * vertex MetalShaderVertexImp::VertexOut vertex_function_entry(
 *   MetalShaderVertexImp::VertexIn v_in [[stage_in]],
 *   constant PushConstantBlock& globals [[buffer(MTL_uniform_buffer_base_index)]]) {
 *
 *   MetalShaderVertexImp impl;
 *   -- Copy input members into impl instance --
 *   -- Execute GLSL main function --
 *   impl.main();
 *
 *   -- Copy outputs and return --
 *   MetalShaderVertexImp::VertexOut out;
 *   out.pos = impl.pos;
 *   -- transform position to Metal coordinate system --
 *   return v_out;
 * }
 * \endcode
 *
 * -- Metal buffer bindings structure --
 *
 * Metal shader contains several different binding types. All buffers are bound using the buffer(N)
 * binding attribute tag. However, different ranges serve different purposes. The structure of the
 * bindings always happen as follows:
 *
 * Vertex Buffers (N)                       <-- 0
 * Index buffer
 * Default Push constant block for uniforms <-- MTL_uniform_buffer_base_index
 * Uniform buffers                          <-- MTL_uniform_buffer_base_index+1
 * Storage buffers                          <-- MTL_storage_buffer_base_index
 * Samplers/argument buffer table           <-- last buffer + 1
 *
 * Up to a maximum of 31 bindings.
 */

namespace blender::gpu {

struct MSLUniform {
  shader::Type type;
  std::string name;
  bool is_array;
  int array_elems;
  ShaderStage stage;

  MSLUniform(shader::Type uniform_type,
             std::string uniform_name,
             bool is_array_type,
             uint32_t num_elems = 1)
      : type(uniform_type), name(uniform_name), is_array(is_array_type), array_elems(num_elems)
  {
  }

  bool operator==(const MSLUniform &right) const
  {
    return (type == right.type && name == right.name && is_array == right.is_array &&
            array_elems == right.array_elems);
  }
};

struct MSLConstant {
  shader::Type type;
  std::string name;

  MSLConstant(shader::Type const_type, std::string const_name) : type(const_type), name(const_name)
  {
  }
};

struct MSLBufferBlock {
  std::string type_name;
  std::string name;
  ShaderStage stage;
  bool is_array;
  /* Resource index in buffer. */
  uint slot;
  uint location;
  shader::Qualifier qualifiers;
  /* Flag for use with texture atomic fallback. */
  bool is_texture_buffer = false;

  bool operator==(const MSLBufferBlock &right) const
  {
    return (type_name == right.type_name && name == right.name);
  }
};

enum MSLTextureSamplerAccess {
  TEXTURE_ACCESS_NONE = 0,
  TEXTURE_ACCESS_SAMPLE,
  TEXTURE_ACCESS_READ,
  TEXTURE_ACCESS_WRITE,
  TEXTURE_ACCESS_READWRITE,
};

struct MSLTextureResource {
  ShaderStage stage;
  shader::ImageType type;
  std::string name;
  MSLTextureSamplerAccess access;
  /* Whether resource is a texture sampler or an image. */
  bool is_texture_sampler;
  /* Index in shader bind table `[[texture(N)]]`. */
  uint slot;
  /* Explicit bind index provided by ShaderCreateInfo. */
  uint location;

  /* Atomic fallback buffer information. */
  int atomic_fallback_buffer_ssbo_id = -1;

  eGPUTextureType get_texture_binding_type() const;
  eGPUSamplerFormat get_sampler_format() const;

  void resolve_binding_indices();

  bool operator==(const MSLTextureResource &right) const
  {
    /* We do not compare stage as we want to avoid duplication of resources used across multiple
     * stages. */
    return (type == right.type && name == right.name && access == right.access);
  }

  std::string get_msl_access_str() const
  {
    switch (access) {
      case TEXTURE_ACCESS_SAMPLE:
        return "access::sample";
      case TEXTURE_ACCESS_READ:
        return "access::read";
      case TEXTURE_ACCESS_WRITE:
        return "access::write";
      case TEXTURE_ACCESS_READWRITE:
        return "access::read_write";
      default:
        BLI_assert(false);
        return "";
    }
    return "";
  }

  /* Get typestring for wrapped texture class members.
   * wrapper struct type contains combined texture and sampler, templated
   * against the texture type.
   * See `COMBINED_SAMPLER_TYPE` in `mtl_shader_defines.msl`. */
  std::string get_msl_typestring_wrapper(bool is_addr) const
  {
    std::string str;
    str = this->get_msl_wrapper_type_str() + "<" + this->get_msl_return_type_str() + "," +
          this->get_msl_access_str() + ">" + ((is_addr) ? "* " : " ") + this->name;
    return str;
  }

  /* Get raw texture typestring -- used in entry-point function argument table. */
  std::string get_msl_typestring(bool is_addr) const
  {
    std::string str;
    str = this->get_msl_texture_type_str() + "<" + this->get_msl_return_type_str() + "," +
          this->get_msl_access_str() + ">" + ((is_addr) ? "* " : " ") + this->name;
    return str;
  }

  std::string get_msl_return_type_str() const;
  std::string get_msl_texture_type_str() const;
  std::string get_msl_wrapper_type_str() const;
};

struct MSLVertexInputAttribute {
  /* layout_location of -1 means unspecified and will
   * be populated manually. */
  int layout_location;
  shader::Type type;
  std::string name;

  bool operator==(const MSLVertexInputAttribute &right) const
  {
    return (layout_location == right.layout_location && type == right.type && name == right.name);
  }
};

struct MSLVertexOutputAttribute {
  std::string type;
  std::string name;
  /* Instance name specified if attributes belong to a struct. */
  std::string instance_name;
  /* Interpolation qualifier can be any of smooth (default), flat, no_perspective. */
  std::string interpolation_qualifier;
  bool is_array;
  int array_elems;

  bool operator==(const MSLVertexOutputAttribute &right) const
  {
    return (type == right.type && name == right.name &&
            interpolation_qualifier == right.interpolation_qualifier &&
            is_array == right.is_array && array_elems == right.array_elems);
  }
  std::string get_mtl_interpolation_qualifier() const
  {
    if (interpolation_qualifier.empty() || interpolation_qualifier == "smooth") {
      return "";
    }
    if (interpolation_qualifier == "flat") {
      return " [[flat]]";
    }
    if (interpolation_qualifier == "noperspective") {
      return " [[center_no_perspective]]";
    }
    return "";
  }
};

struct MSLFragmentOutputAttribute {
  /* Explicit output binding location N for [[color(N)]] -1 = unspecified. */
  int layout_location;
  /* Output index for dual source blending. -1 = unspecified. */
  int layout_index;
  shader::Type type;
  std::string name;
  /* Raster order group can be specified to synchronize pixel read and write operations between
   * subsequent draws. If a subsequent draw requires reading data from a GBuffer, raster order
   * groups should be used to ensure all writes occur before reading. */
  int raster_order_group;
  /* Used for lack of ROG support workaround. */
  bool is_layered_input;

  bool operator==(const MSLFragmentOutputAttribute &right) const
  {
    return (layout_location == right.layout_location && type == right.type && name == right.name &&
            layout_index == right.layout_index && raster_order_group == right.raster_order_group);
  }
};

/* Fragment tile inputs match fragment output attribute layout. */
using MSLFragmentTileInputAttribute = MSLFragmentOutputAttribute;

struct MSLSharedMemoryBlock {
  /* e.g. `shared vec4 color_cache[cache_size][cache_size];`. */
  std::string type_name;
  std::string varname;
  bool is_array;
  std::string array_decl; /* String containing array declaration. e.g. [cache_size][cache_size]. */
};

class MSLGeneratorInterface {
  static char *msl_patch_default;

 public:
  /** Shader stage input/output binding information.
   * Derived from shader source reflection or GPUShaderCreateInfo. */
  blender::Vector<MSLBufferBlock> uniform_blocks;
  blender::Vector<MSLBufferBlock> storage_blocks;
  blender::Vector<MSLUniform> uniforms;
  blender::Vector<MSLTextureResource> texture_samplers;
  blender::Vector<MSLVertexInputAttribute> vertex_input_attributes;
  blender::Vector<MSLVertexOutputAttribute> vertex_output_varyings;
  /* Specialization Constants. */
  blender::Vector<MSLConstant> constants;
  /* Fragment tile inputs. */
  blender::Vector<MSLFragmentTileInputAttribute> fragment_tile_inputs;
  /* Should match vertex outputs, but defined separately as
   * some shader permutations will not utilize all inputs/outputs.
   * Final shader uses the intersection between the two sets. */
  blender::Vector<MSLVertexOutputAttribute> fragment_input_varyings;
  blender::Vector<MSLFragmentOutputAttribute> fragment_outputs;
  /* Clip Distances. */
  blender::Vector<char> clip_distances;
  /* Max bind IDs. */
  int max_tex_bind_index = 0;
  /** GL Global usage. */
  /* Whether GL position is used, or an alternative vertex output should be the default. */
  bool uses_gl_Position;
  /* Whether gl_FragColor is used, or whether an alternative fragment output
   * should be the default. */
  bool uses_gl_FragColor;
  /* Whether gl_PointCoord is used in the fragment shader. If so,
   * we define float2 gl_PointCoord [[point_coord]]. */
  bool uses_gl_PointCoord;
  /* Writes out to gl_PointSize in the vertex shader output. */
  bool uses_gl_PointSize;
  bool uses_gl_VertexID;
  bool uses_gl_InstanceID;
  bool uses_gl_BaseInstanceARB;
  bool uses_gl_FrontFacing;
  bool uses_gl_PrimitiveID;
  /* Sets the output render target array index when using multilayered rendering. */
  bool uses_gl_FragDepth;
  bool uses_gl_FragStencilRefARB;
  bool uses_gpu_layer;
  bool uses_gpu_viewport_index;
  bool uses_barycentrics;
  /* Compute shader global variables. */
  bool uses_gl_GlobalInvocationID;
  bool uses_gl_WorkGroupSize;
  bool uses_gl_WorkGroupID;
  bool uses_gl_NumWorkGroups;
  bool uses_gl_LocalInvocationIndex;
  bool uses_gl_LocalInvocationID;
  /* Early fragment tests. */
  bool uses_early_fragment_test;

  /* Parameters. */
  shader::DepthWrite depth_write;

  /* Bind index trackers. */
  int max_buffer_slot = 0;

  /* Shader buffer bind indices for argument buffers per shader stage.
   * NOTE: Compute stage will re-use index 0. */
  int sampler_argument_buffer_bind_index[3] = {-1, -1, -1};

 private:
  /* Parent shader instance. */
  MTLShader &parent_shader_;

  /* If prepared from Create info. */
  const shader::ShaderCreateInfo *create_info_;

 public:
  MSLGeneratorInterface(MTLShader &shader) : parent_shader_(shader){};

  /** Prepare MSLGeneratorInterface from create-info. **/
  void prepare_from_createinfo(const shader::ShaderCreateInfo *info);

  /* Samplers. */
  bool use_argument_buffer_for_samplers() const;
  uint32_t num_samplers_for_stage(ShaderStage stage) const;
  uint32_t max_sampler_index_for_stage(ShaderStage stage) const;

  /* Returns the bind index, relative to
   * MTL_uniform_buffer_base_index+MTL_storage_buffer_base_index. */
  uint32_t get_sampler_argument_buffer_bind_index(ShaderStage stage);

  /* Code generation utility functions. */
  std::string generate_msl_uniform_structs(ShaderStage shader_stage);
  std::string generate_msl_vertex_in_struct();
  std::string generate_msl_vertex_out_struct(ShaderStage shader_stage);
  std::string generate_msl_fragment_struct(bool is_input);
  std::string generate_msl_vertex_inputs_string();
  std::string generate_msl_fragment_inputs_string();
  std::string generate_msl_compute_inputs_string();
  std::string generate_msl_vertex_entry_stub();
  std::string generate_msl_fragment_entry_stub();
  std::string generate_msl_compute_entry_stub();
  std::string generate_msl_fragment_tile_input_population();
  std::string generate_msl_global_uniform_population(ShaderStage stage);
  std::string generate_ubo_block_macro_chain(MSLBufferBlock block);
  std::string generate_msl_uniform_block_population(ShaderStage stage);
  std::string generate_msl_vertex_attribute_input_population();
  std::string generate_msl_vertex_output_population();
  std::string generate_msl_fragment_input_population();
  std::string generate_msl_fragment_output_population();
  std::string generate_msl_uniform_undefs(ShaderStage stage);
  std::string generate_ubo_block_undef_chain(ShaderStage stage);
  std::string generate_msl_texture_vars(ShaderStage shader_stage);
  void generate_msl_textures_input_string(std::stringstream &out,
                                          ShaderStage stage,
                                          bool &is_first_parameter);
  void generate_msl_uniforms_input_string(std::stringstream &out,
                                          ShaderStage stage,
                                          bool &is_first_parameter);

  /* Location is not always specified, so this will resolve outstanding locations. */
  void resolve_input_attribute_locations();
  void resolve_fragment_output_locations();

  /* Create shader interface for converted GLSL shader. */
  MTLShaderInterface *bake_shader_interface(const char *name,
                                            const shader::ShaderCreateInfo *info = nullptr);

  /* Fetch combined shader source header. */
  char *msl_patch_default_get();

  MEM_CXX_CLASS_ALLOC_FUNCS("MSLGeneratorInterface");
};

inline const char *get_stage_class_name(ShaderStage stage)
{
  switch (stage) {
    case ShaderStage::VERTEX:
      return "MTLShaderVertexImpl";
    case ShaderStage::FRAGMENT:
      return "MTLShaderFragmentImpl";
    case ShaderStage::COMPUTE:
      return "MTLShaderComputeImpl";
    default:
      BLI_assert_unreachable();
      return "";
  }
  return "";
}

inline const char *get_shader_stage_instance_name(ShaderStage stage)
{
  switch (stage) {
    case ShaderStage::VERTEX:
      return "vertex_shader_instance";
    case ShaderStage::FRAGMENT:
      return "fragment_shader_instance";
    case ShaderStage::COMPUTE:
      return "compute_shader_instance";
    default:
      BLI_assert_unreachable();
      return "";
  }
  return "";
}

inline bool is_builtin_type(std::string type)
{
  /* Add Types as needed. */
  /* TODO(Metal): Consider replacing this with a switch and `constexpr` hash and switch.
   * Though most efficient and maintainable approach to be determined.
   * NOTE: Some duplicate types exit for Metal and GLSL representations, as generated type-names
   * from #shader::ShaderCreateInfo may use GLSL signature. */
  static std::map<std::string, eMTLDataType> glsl_builtin_types = {
      {"float", MTL_DATATYPE_FLOAT},
      {"vec2", MTL_DATATYPE_FLOAT2},
      {"vec3", MTL_DATATYPE_FLOAT3},
      {"vec4", MTL_DATATYPE_FLOAT4},
      {"int", MTL_DATATYPE_INT},
      {"ivec2", MTL_DATATYPE_INT2},
      {"ivec3", MTL_DATATYPE_INT3},
      {"ivec4", MTL_DATATYPE_INT4},
      {"int2", MTL_DATATYPE_INT2},
      {"int3", MTL_DATATYPE_INT3},
      {"int4", MTL_DATATYPE_INT4},
      {"uint32_t", MTL_DATATYPE_UINT},
      {"uvec2", MTL_DATATYPE_UINT2},
      {"uvec3", MTL_DATATYPE_UINT3},
      {"uvec4", MTL_DATATYPE_UINT4},
      {"uint", MTL_DATATYPE_UINT},
      {"uint2", MTL_DATATYPE_UINT2},
      {"uint3", MTL_DATATYPE_UINT3},
      {"uint4", MTL_DATATYPE_UINT4},
      {"mat3", MTL_DATATYPE_FLOAT3x3},
      {"mat4", MTL_DATATYPE_FLOAT4x4},
      {"bool", MTL_DATATYPE_INT},
      {"uchar", MTL_DATATYPE_UCHAR},
      {"uchar2", MTL_DATATYPE_UCHAR2},
      {"uchar2", MTL_DATATYPE_UCHAR3},
      {"uchar4", MTL_DATATYPE_UCHAR4},
      {"vec3_1010102_Unorm", MTL_DATATYPE_UINT1010102_NORM},
      {"vec3_1010102_Inorm", MTL_DATATYPE_INT1010102_NORM},
      {"packed_float2", MTL_DATATYPE_PACKED_FLOAT2},
      {"packed_float3", MTL_DATATYPE_PACKED_FLOAT3},
  };
  return (glsl_builtin_types.find(type) != glsl_builtin_types.end());
}

inline bool is_matrix_type(const std::string &type)
{
  /* Matrix type support. Add types as necessary. */
  return (type == "mat4");
}

inline bool is_matrix_type(const shader::Type &type)
{
  /* Matrix type support. Add types as necessary. */
  return (type == shader::Type::float4x4_t || type == shader::Type::float3x3_t);
}

inline int get_matrix_location_count(const std::string &type)
{
  /* Matrix type support. Add types as necessary. */
  if (type == "mat4") {
    return 4;
  }
  if (type == "mat3") {
    return 3;
  }
  return 1;
}

inline int get_matrix_location_count(const shader::Type &type)
{
  /* Matrix type support. Add types as necessary. */
  if (type == shader::Type::float4x4_t) {
    return 4;
  }
  if (type == shader::Type::float3x3_t) {
    return 3;
  }
  return 1;
}

inline std::string get_matrix_subtype(const std::string &type)
{
  if (type == "mat4") {
    return "vec4";
  }
  return type;
}

inline shader::Type get_matrix_subtype(const shader::Type &type)
{
  if (type == shader::Type::float4x4_t) {
    return shader::Type::float4_t;
  }
  if (type == shader::Type::float3x3_t) {
    return shader::Type::float3_t;
  }
  return type;
}

inline std::string get_attribute_conversion_function(bool *uses_conversion,
                                                     const shader::Type &type)
{
  /* NOTE(Metal): Add more attribute types as required. */
  if (type == shader::Type::float_t) {
    *uses_conversion = true;
    return "internal_vertex_attribute_convert_read_float";
  }
  if (type == shader::Type::float2_t) {
    *uses_conversion = true;
    return "internal_vertex_attribute_convert_read_float2";
  }
  if (type == shader::Type::float3_t) {
    *uses_conversion = true;
    return "internal_vertex_attribute_convert_read_float3";
  }
  if (type == shader::Type::float4_t) {
    *uses_conversion = true;
    return "internal_vertex_attribute_convert_read_float4";
  }
  *uses_conversion = false;
  return "";
}

inline const char *to_string(const shader::PrimitiveOut &layout)
{
  switch (layout) {
    case shader::PrimitiveOut::POINTS:
      return "points";
    case shader::PrimitiveOut::LINE_STRIP:
      return "line_strip";
    case shader::PrimitiveOut::TRIANGLE_STRIP:
      return "triangle_strip";
    default:
      BLI_assert(false);
      return "unknown";
  }
}

inline const char *to_string(const shader::PrimitiveIn &layout)
{
  switch (layout) {
    case shader::PrimitiveIn::POINTS:
      return "points";
    case shader::PrimitiveIn::LINES:
      return "lines";
    case shader::PrimitiveIn::LINES_ADJACENCY:
      return "lines_adjacency";
    case shader::PrimitiveIn::TRIANGLES:
      return "triangles";
    case shader::PrimitiveIn::TRIANGLES_ADJACENCY:
      return "triangles_adjacency";
    default:
      BLI_assert(false);
      return "unknown";
  }
}

inline const char *to_string(const shader::Interpolation &interp)
{
  switch (interp) {
    case shader::Interpolation::SMOOTH:
      return "smooth";
    case shader::Interpolation::FLAT:
      return "flat";
    case shader::Interpolation::NO_PERSPECTIVE:
      return "noperspective";
    default:
      BLI_assert(false);
      return "unknown";
  }
}

inline const char *to_string_msl(const shader::Interpolation &interp)
{
  switch (interp) {
    case shader::Interpolation::SMOOTH:
      return "[[center_perspective]]";
    case shader::Interpolation::FLAT:
      return "[[flat]]";
    case shader::Interpolation::NO_PERSPECTIVE:
      return "[[center_no_perspective]]";
    default:
      return "";
  }
}

inline const char *to_string(const shader::Type &type)
{
  switch (type) {
    case shader::Type::float_t:
      return "float";
    case shader::Type::float2_t:
      return "vec2";
    case shader::Type::float3_t:
      return "vec3";
    case shader::Type::float3_10_10_10_2_t:
      return "vec3_1010102_Inorm";
    case shader::Type::float4_t:
      return "vec4";
    case shader::Type::float3x3_t:
      return "mat3";
    case shader::Type::float4x4_t:
      return "mat4";
    case shader::Type::uint_t:
      return "uint32_t";
    case shader::Type::uint2_t:
      return "uvec2";
    case shader::Type::uint3_t:
      return "uvec3";
    case shader::Type::uint4_t:
      return "uvec4";
    case shader::Type::int_t:
      return "int";
    case shader::Type::int2_t:
      return "ivec2";
    case shader::Type::int3_t:
      return "ivec3";
    case shader::Type::int4_t:
      return "ivec4";
    case shader::Type::bool_t:
      return "bool";
    case shader::Type::uchar_t:
      return "uchar";
    case shader::Type::uchar2_t:
      return "uchar2";
    case shader::Type::uchar3_t:
      return "uchar3";
    case shader::Type::uchar4_t:
      return "uchar4";
    case shader::Type::char_t:
      return "char";
    case shader::Type::char2_t:
      return "char2";
    case shader::Type::char3_t:
      return "char3";
    case shader::Type::char4_t:
      return "char4";
    case shader::Type::ushort_t:
      return "ushort";
    case shader::Type::ushort2_t:
      return "ushort2";
    case shader::Type::ushort3_t:
      return "ushort3";
    case shader::Type::ushort4_t:
      return "ushort4";
    case shader::Type::short_t:
      return "short";
    case shader::Type::short2_t:
      return "short2";
    case shader::Type::short3_t:
      return "short3";
    case shader::Type::short4_t:
      return "short4";
    default:
      BLI_assert(false);
      return "unknown";
  }
}

inline char *next_symbol_in_range(char *begin, const char *end, char symbol)
{
  for (char *a = begin; a < end; a++) {
    if (*a == symbol) {
      return a;
    }
  }
  return nullptr;
}

inline char *next_word_in_range(char *begin, const char *end)
{
  for (char *a = begin; a < end; a++) {
    char chr = *a;
    if ((chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z') || (chr >= '0' && chr <= '9') ||
        (chr == '_'))
    {
      return a;
    }
  }
  return nullptr;
}

}  // namespace blender::gpu

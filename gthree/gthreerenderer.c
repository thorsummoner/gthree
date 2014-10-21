#include <math.h>
#include <epoxy/gl.h>

#include "gthreerenderer.h"
#include "gthreeobjectprivate.h"
#include "gthreeshader.h"
#include "gthreematerial.h"
#include "gthreeutils.h"

typedef struct {
  int width;
  int height;
  gboolean auto_clear;
  gboolean auto_clear_color;
  gboolean auto_clear_depth;
  gboolean auto_clear_stencil;
  GdkRGBA clear_color;
  gboolean sort_objects;
  GthreeMaterial *override_material;

  float viewport_x;
  float viewport_y;
  float viewport_width;
  float viewport_height;

  /* Render state */
  GthreeFrustum frustum;
  graphene_matrix_t proj_screen_matrix;

  guint used_texture_units;

  gboolean old_flip_sided;
  gboolean old_double_sided;
  gboolean old_depth_test;
  gboolean old_depth_write;
  float old_line_width;
  gboolean old_polygon_offset;
  float old_polygon_offset_factor;
  float old_polygon_offset_units;
  GthreeBlendMode old_blending;
  guint old_blend_equation;
  guint old_blend_src;
  guint old_blend_dst;
  GthreeProgram *current_program;
  GthreeMaterial *current_material;
  GthreeCamera *current_camera;

  GPtrArray *opaque_objects;
  GPtrArray *transparent_objects;

  guint8 new_attributes[8];
  guint8 enabled_attributes[8];

  int max_textures;
  int max_vertex_textures;
  int max_texture_size;
  int max_cubemap_size;
  float max_anisotropy;

  gboolean supports_vertex_textures;
  gboolean supports_bone_textures;

} GthreeRendererPrivate;

static void gthree_set_default_gl_state (GthreeRenderer *renderer);

G_DEFINE_TYPE_WITH_PRIVATE (GthreeRenderer, gthree_renderer, G_TYPE_OBJECT);

GthreeRenderer *
gthree_renderer_new ()
{
  GthreeRenderer *renderer;

  renderer = g_object_new (gthree_renderer_get_type (),
                           NULL);

  return renderer;
}

static void
gthree_renderer_init (GthreeRenderer *renderer)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);

  priv->auto_clear = TRUE;
  priv->auto_clear_color = TRUE;
  priv->auto_clear_depth = TRUE;
  priv->auto_clear_stencil = TRUE;
  priv->sort_objects = TRUE;
  priv->width = 1;
  priv->height = 1;

  priv->opaque_objects = g_ptr_array_new ();
  priv->transparent_objects = g_ptr_array_new ();

  priv->old_blending = -1;
  priv->old_blend_equation = -1;
  priv->old_blend_src = -1;
  priv->old_blend_dst = -1;

  gthree_set_default_gl_state (renderer);

  // GPU capabilities
  glGetIntegerv (GL_MAX_TEXTURE_IMAGE_UNITS, &priv->max_textures);
  glGetIntegerv (GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &priv->max_vertex_textures);
  glGetIntegerv (GL_MAX_TEXTURE_SIZE, &priv->max_texture_size);
  glGetIntegerv (GL_MAX_CUBE_MAP_TEXTURE_SIZE, &priv->max_cubemap_size);

  priv->max_anisotropy = 0.0f;
  if (epoxy_has_gl_extension("GL_EXT_texture_filter_anisotropic"))
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &priv->max_anisotropy);

  priv->supports_vertex_textures = priv->max_vertex_textures > 0;
  priv->supports_bone_textures =
    priv->supports_vertex_textures &&
    epoxy_has_gl_extension("GL_ARB_texture_float");

  //priv->compressed_texture_formats = _glExtensionCompressedTextureS3TC ? glGetParameter( _gl.COMPRESSED_TEXTURE_FORMATS ) : [];

}

static void
gthree_renderer_finalize (GObject *obj)
{
  //GthreeRenderer *renderer = GTHREE_RENDERER (obj);
  //GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);

  G_OBJECT_CLASS (gthree_renderer_parent_class)->finalize (obj);
}

static void
gthree_renderer_class_init (GthreeRendererClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = gthree_renderer_finalize;
}

void
gthree_renderer_set_viewport (GthreeRenderer *renderer,
                              float           x,
                              float           y,
                              float           width,
                              float           height)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);

  priv->viewport_x = x;
  priv->viewport_y = y;
  priv->viewport_width = width;
  priv->viewport_height = height;

  glViewport (x, y, width, height);
}

void
gthree_renderer_set_size (GthreeRenderer *renderer,
                          int             width,
                          int             height)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);

  priv->width = width;
  priv->height = height;

  gthree_renderer_set_viewport (renderer, 0, 0, width, height);
}

void
gthree_renderer_set_autoclear (GthreeRenderer *renderer,
                               gboolean        auto_clear)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);

  priv->auto_clear = !!auto_clear;
}

void
gthree_renderer_set_clear_color (GthreeRenderer *renderer,
                                 GdkRGBA        *color)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);

  priv->clear_color = *color;

  glClearColor (priv->clear_color.red, priv->clear_color.green, priv->clear_color.blue, priv->clear_color.alpha );
}

static void
gthree_set_default_gl_state (GthreeRenderer *renderer)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);

  glClearColor (0, 0, 0, 1);
  glClearDepth (1);
  glClearStencil (0);

  glEnable (GL_DEPTH_TEST);
  glDepthFunc (GL_LEQUAL);

  glFrontFace (GL_CCW);
  glCullFace (GL_BACK);
  glEnable (GL_CULL_FACE);

  glEnable (GL_BLEND);
  glBlendEquation (GL_FUNC_ADD);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glViewport (priv->viewport_x, priv->viewport_y, priv->viewport_width, priv->viewport_height);
  glClearColor (priv->clear_color.red, priv->clear_color.green, priv->clear_color.blue, priv->clear_color.alpha );
};

void
gthree_renderer_clear (GthreeRenderer *renderer)
{
}

static gint
painter_sort_stable (gconstpointer  a, gconstpointer  b)
{
  /* TODO */
  return 0;
}

static gint
reverse_painter_sort_stable (gconstpointer  a, gconstpointer  b)
{
  /* TODO */
  return 0;
}

static void
set_material_faces (GthreeRenderer *renderer,
                    GthreeMaterial *material)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);
  GthreeSide side = gthree_material_get_side (material);
  gboolean double_sided = side == GTHREE_SIDE_DOUBLE;
  gboolean flip_sided = side == GTHREE_SIDE_BACK;

  if (priv->old_double_sided != double_sided )
    {
      if (double_sided)
        glDisable (GL_CULL_FACE);
      else
        glEnable (GL_CULL_FACE);

      priv->old_double_sided = double_sided;
    }

  if (priv->old_flip_sided != flip_sided ) {
    if (flip_sided)
      glFrontFace (GL_CW);
    else
      glFrontFace (GL_CCW);

    priv->old_flip_sided = flip_sided;
  }
}

static void
set_depth_test (GthreeRenderer *renderer,
                gboolean depth_test)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);

  if (priv->old_depth_test != depth_test)
    {
      if (depth_test)
        glEnable (GL_DEPTH_TEST);
      else
        glDisable (GL_DEPTH_TEST);

      priv->old_depth_test = depth_test;
    }
}

static void
set_depth_write (GthreeRenderer *renderer,
                gboolean depth_write)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);

  if (priv->old_depth_write != depth_write)
    {
      glDepthMask (depth_write);
      priv->old_depth_write = depth_write;
    }
}

static void
set_line_width (GthreeRenderer *renderer,
                float line_width)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);

  if (priv->old_line_width != line_width)
    {
      glLineWidth (line_width);
      priv->old_line_width = line_width;
    }
}

static void
set_polygon_offset (GthreeRenderer *renderer,
                    gboolean polygon_offset,
                    float factor, float units)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);

  if (priv->old_polygon_offset != polygon_offset)
    {
      if (polygon_offset)
        glEnable (GL_POLYGON_OFFSET_FILL);
      else
        glDisable (GL_POLYGON_OFFSET_FILL);

      priv->old_polygon_offset = polygon_offset;
    }

  if (polygon_offset && (priv->old_polygon_offset_factor != factor || priv->old_polygon_offset_units != units ))
    {
      glPolygonOffset (factor, units);
      priv->old_polygon_offset_factor = factor;
      priv->old_polygon_offset_units = units;
  }
}

static void
set_blending (GthreeRenderer *renderer,
              GthreeBlendMode blending,
              guint blend_equation,
              guint blend_src,
              guint blend_dst)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);

  if (blending != priv->old_blending)
    {
      switch (blending)
        {
        default:
        case GTHREE_BLEND_NO:
          glDisable (GL_BLEND);
          break;

        case GTHREE_BLEND_NORMAL:
          glEnable (GL_BLEND);
          glBlendEquationSeparate (GL_FUNC_ADD, GL_FUNC_ADD);
          glBlendFuncSeparate (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
          break;

        case GTHREE_BLEND_ADDITIVE:
          glEnable (GL_BLEND);
          glBlendEquation (GL_FUNC_ADD);
          glBlendFunc (GL_SRC_ALPHA, GL_ONE);
          break;

        case GTHREE_BLEND_SUBTRACTIVE:
          // TODO: Find blendFuncSeparate() combination
          glEnable (GL_BLEND);
          glBlendEquation (GL_FUNC_ADD);
          glBlendFunc (GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
          break;

        case GTHREE_BLEND_MULTIPLY:
          // TODO: Find blendFuncSeparate() combination
          glEnable (GL_BLEND);
          glBlendEquation (GL_FUNC_ADD);
          glBlendFunc (GL_ZERO, GL_SRC_COLOR);
          break;

        case GTHREE_BLEND_CUSTOM:
          glEnable (GL_BLEND);
          break;
        }
      priv->old_blending = blending;
    }

  if (blending == GTHREE_BLEND_CUSTOM)
    {
      if (blend_equation != priv->old_blend_equation)
        {
          glBlendEquation (blend_equation);
          priv->old_blend_equation = blend_equation;
        }

      if (blend_src != priv->old_blend_src || blend_dst != priv->old_blend_dst)
        {
          glBlendFunc (blend_src, blend_dst);

          priv->old_blend_src = blend_src;
          priv->old_blend_dst = blend_dst;
        }
    }
  else
    {
      priv->old_blend_equation = -1;
      priv->old_blend_src = -1;
      priv->old_blend_dst = -1;
    }
}

static void
resolve_buffer_material (GthreeRenderer *renderer,
                         GthreeBuffer *buffer)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);
  GthreeMaterial *material = buffer->material;

  if (material)
    {
      if (gthree_material_get_is_transparent (material))
        g_ptr_array_add (priv->transparent_objects, buffer);
      else
        g_ptr_array_add (priv->opaque_objects, buffer);
    }
}

static void
project_object (GthreeRenderer *renderer,
                GthreeScene    *scene,
                GthreeObject   *object,
                GthreeCamera   *camera)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);
  GList *l, *buffers;
  GthreeObject *child;
  GthreeObjectIter iter;

  if (!gthree_object_get_visible (object))
    return;

  buffers = gthree_object_get_buffers (object);

  if (buffers != NULL && (!gthree_object_get_is_frustum_culled (object) || gthree_object_is_in_frustum (object, &priv->frustum)))
    {
      gthree_object_update (object);

      for (l = buffers; l != NULL; l = l->next)
        {
          GthreeBuffer *buffer = l->data;

          resolve_buffer_material (renderer, buffer);

          if (priv->sort_objects)
            {
              /* TODO
              if (object.renderDepth != null)
                {
                  buffer->z = object.renderDepth;
                }
                else */
                {
                  graphene_vec4_t vector;

                  /* Get position */
                  graphene_matrix_get_row (gthree_object_get_world_matrix (object), 3, &vector);

                  /* project object position to screen */
                  graphene_matrix_transform_vec4 (&priv->proj_screen_matrix, &vector, &vector);
                  graphene_vec4_normalize (&vector, &vector);

                  buffer->z = graphene_vec4_get_z (&vector);
                }
            }
        }
    }

  gthree_object_iter_init (&iter, object);
  while (gthree_object_iter_next (&iter, &child))
    project_object (renderer, scene, child, camera);
}

static void
deallocate_material (GthreeRenderer *renderer,
                     GthreeMaterial *material)
{
  // TODO
}

static GthreeProgram *
init_material (GthreeRenderer *renderer,
               GthreeMaterial *material,
               gpointer light,
               gpointer fog,
               GthreeObject *object)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);
  char *shader_id;
  GthreeProgram *program;
  GthreeShader *shader;
  GthreeProgramParameters parameters = {0};
  GList  *unis, *l;
  gpointer code;

  shader = gthree_material_get_shader (material);

  //material.addEventListener( 'dispose', onMaterialDispose );
  //var u, a, identifiers, i, parameters, maxLightCount, maxBones, maxShadows, shaderID;

  // heuristics to create shader parameters according to lights in the scene
  // (not to blow over maxLights budget)

#ifdef TODO
  maxLightCount = allocateLights( lights );
  maxShadows = allocateShadows( lights );
  maxBones = allocateBones( object );
#endif

  parameters.precision = GTHREE_PRECISION_HIGH;
  parameters.supports_vertex_textures = priv->supports_vertex_textures;

  gthree_material_set_params (material, &parameters);


#ifdef TODO
  parameters =
    {
    precision: _precision,

    map: !! material.map,
    envMap: !! material.envMap,
    lightMap: !! material.lightMap,
    bumpMap: !! material.bumpMap,
    normalMap: !! material.normalMap,
    specularMap: !! material.specularMap,
    alphaMap: !! material.alphaMap,

    vertexColors: material.vertexColors,

    fog: fog,
    useFog: material.fog,
    fogExp: fog instanceof THREE.FogExp2,

    sizeAttenuation: material.sizeAttenuation,
    logarithmicDepthBuffer: _logarithmicDepthBuffer,

    skinning: material.skinning,
    maxBones: maxBones,
    useVertexTexture: _supportsBoneTextures && object && object.skeleton && object.skeleton.useVertexTexture,

    morphTargets: material.morphTargets,
    morphNormals: material.morphNormals,
    maxMorphTargets: this.maxMorphTargets,
    maxMorphNormals: this.maxMorphNormals,

    maxDirLights: maxLightCount.directional,
    maxPointLights: maxLightCount.point,
    maxSpotLights: maxLightCount.spot,
    maxHemiLights: maxLightCount.hemi,

    maxShadows: maxShadows,
    shadowMapEnabled: this.shadowMapEnabled && object.receiveShadow && maxShadows > 0,
    shadowMapType: this.shadowMapType,
    shadowMapDebug: this.shadowMapDebug,
    shadowMapCascade: this.shadowMapCascade,

    alphaTest: material.alphaTest,
    metal: material.metal,
    wrapAround: material.wrapAround,
    };
#endif

  code = NULL;

#ifdef TODO

  // Generate code
  var chunks = [];

  if (shader_id)
    {
      chunks.push( shader_id );
    }
  else
    {
      chunks.push( material.fragmentShader );
      chunks.push( material.vertexShader );
    }

  for ( var d in material.defines )
    {
      chunks.push( d );
      chunks.push( material.defines[ d ] );
    }

  for ( var p in parameters )
    {
      chunks.push( p );
      chunks.push( parameters[ p ] );
    }

  var code = chunks.join();
#endif

  program = NULL;

#ifdef TODO
  // Check if code has been already compiled
  for ( var p = 0, pl = _programs.length; p < pl; p ++ )
    {
      var programInfo = _programs[ p ];
      if (programInfo.code == code)
        {
          program = programInfo;
          program.usedTimes ++;
          break;
        }
    }
#endif

  if (program == NULL)
    {
      program = gthree_program_new (code, material, &parameters);
      //_programs.push( program );
    }

  material->program = program;
  
#ifdef TODO
  var attributes = material.program.attributes;
  if (material.morphTargets)
    {
      material.numSupportedMorphTargets = 0;
      var id, base = 'morphTarget';
      for ( i = 0; i < this.maxMorphTargets; i ++ )
        {
          id = base + i;
          if ( attributes[ id ] >= 0 )
            {
              material.numSupportedMorphTargets ++;
            }

        }
    }

  if (material.morphNormals )
    {
      material.numSupportedMorphNormals = 0;
      var id, base = 'morphNormal';

      for ( i = 0; i < this.maxMorphNormals; i ++ )
        {
          id = base + i;
          if ( attributes[ id ] >= 0 )
            material.numSupportedMorphNormals ++;
        }
    }
#endif

  /* TODO: Move into shader */
  unis = gthree_uniforms_get_all (shader->uniforms);
  for (l = unis; l != NULL; l = l->next)
    {
      GthreeUniform *uni = l->data;
      gint location;

      location = gthree_program_lookup_uniform_location (program, gthree_uniform_get_name (uni));
      gthree_uniform_set_location (uni, location);
    }
  g_list_free (unis);

  return NULL;
}

#if 0
static void
print_matrix4 (float *s)
{
  int i,j;
  for (i = 0; i < 4; i++)
    {
      if (i == 0)
        g_print("[ ");
      else
        g_print("  ");
      for (j = 0; j < 4; j++)
        {
          if (j != 0)
            g_print (", ");
          g_print ("%f", *s++);
        }
      if (i == 3)
        g_print("]\n");
      else
        g_print("\n");
    }
}
#endif

static void
load_uniforms_matrices (GthreeRenderer *renderer,
                        GthreeProgram *program,
                        GthreeObject *object)
{
  float matrix[16];
  int mvm_location = gthree_program_lookup_uniform_location (program, "modelViewMatrix");

  gthree_object_get_model_view_matrix_floats (object, matrix);
  glUniformMatrix4fv (mvm_location, 1, FALSE, matrix);

#ifdef TODO
  if (uniforms.normalMatrix) {
    glUniformMatrix3fv (uniforms.normalMatrix, false, object._normalMatrix.elements);
  }
#endif
}


static GthreeProgram *
set_program (GthreeRenderer *renderer,
             GthreeCamera *camera,
             gpointer lights,
             gpointer fog,
             GthreeMaterial *material,
             GthreeObject *object)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);
  gboolean refreshProgram = false;
  gboolean refreshMaterial = false;
  gboolean refreshLights = false;
  GthreeProgram *program;
  GthreeShader *shader;
  GthreeUniforms *m_uniforms;
  int location;

  priv->used_texture_units = 0;

  if (material->needs_update)
    {
      if (material->program)
        deallocate_material (renderer, material);
      init_material (renderer, material, lights, fog, object);
      material->needs_update = FALSE;
    }

  program = material->program;
  shader = gthree_material_get_shader (material);
  m_uniforms = shader->uniforms;

  if (program != priv->current_program )
    {
      glUseProgram (gthree_program_get_program (program));
      priv->current_program = program;

      refreshProgram = TRUE;
      refreshMaterial = TRUE;
      refreshLights = TRUE;
    }

  if (material != priv->current_material)
    {
      if (priv->current_material == NULL)
        refreshLights = TRUE;
      priv->current_material = material;
      refreshMaterial = TRUE;
    }

  if (refreshProgram || camera != priv->current_camera)
    {
      const graphene_matrix_t *projection_matrix = gthree_camera_get_projection_matrix (camera);
      float projection_matrixv[16];
      gint proction_matrix_location = gthree_program_lookup_uniform_location (program, "projectionMatrix");

      graphene_matrix_to_float (projection_matrix, projection_matrixv);
      glUniformMatrix4fv (proction_matrix_location, 1, FALSE, projection_matrixv);

#ifdef TODO
      if ( _logarithmicDepthBuffer )
        glUniform1f (uniform_locations.logDepthBufFC, 2.0 / ( Math.log( camera.far + 1.0 ) / Math.LN2 ));

#endif
      if (camera != priv->current_camera)
        priv->current_camera = camera;

      // load material specific uniforms
      // (shader material also gets them for the sake of genericity)

#if TODO
      if (material instanceof THREE.ShaderMaterial ||
          material instanceof THREE.MeshPhongMaterial ||
          material.envMap )
        {
          if ( uniform_locations.cameraPosition !== null )
            {
              _vector3.setFromMatrixPosition( camera.matrixWorld );
              _gl.uniform3f( uniform_locations.cameraPosition, _vector3.x, _vector3.y, _vector3.z );
            }
        }
#endif

#if TODO
      if (material instanceof THREE.MeshPhongMaterial ||
          material instanceof THREE.MeshLambertMaterial ||
          material instanceof THREE.ShaderMaterial ||
          material.skinning)
        {
          if ( uniform_locations.viewMatrix !== null )
            {
              glUniformMatrix4fv (uniform_locations.viewMatrix, false, camera.matrixWorldInverse.elements);
            }
        }
#endif
    }

  // skinning uniforms must be set even if material didn't change
  // auto-setting of texture unit for bone texture must go before other textures
  // not sure why, but otherwise weird things happen

#if TODO
  if ( material.skinning )
    {
      if ( object.bindMatrix && uniform_locations.bindMatrix !== null )
        glUniformMatrix4fv( uniform_locations.bindMatrix, false, object.bindMatrix.elements );

      if ( object.bindMatrixInverse && uniform_locations.bindMatrixInverse !== null )
        glUniformMatrix4fv( uniform_locations.bindMatrixInverse, false, object.bindMatrixInverse.elements );

      if ( _supportsBoneTextures && object.skeleton && object.skeleton.useVertexTexture )
        {
          if ( uniform_locations.boneTexture !== null )
            {
              var textureUnit = getTextureUnit();
              glUniform1i( uniform_locations.boneTexture, textureUnit );
              _this.setTexture( object.skeleton.boneTexture, textureUnit );
            }

          if ( uniform_locations.boneTextureWidth !== null )
            glUniform1i( uniform_locations.boneTextureWidth, object.skeleton.boneTextureWidth );

          if ( uniform_locations.boneTextureHeight !== null )
            glUniform1i( uniform_locations.boneTextureHeight, object.skeleton.boneTextureHeight );

        }
      else if ( object.skeleton && object.skeleton.boneMatrices )
        {
          if ( uniform_locations.boneGlobalMatrices !== null )
            glUniformMatrix4fv( uniform_locations.boneGlobalMatrices, false, object.skeleton.boneMatrices );
        }
    }
#endif

  if ( refreshMaterial )
    {
      gthree_material_set_uniforms (material, m_uniforms);

#if TODO
    // refresh uniforms common to several materials
      if ( fog && material.fog )
        refreshUniformsFog( m_uniforms, fog );
#endif

#if TODO
      if ( material instanceof THREE.MeshPhongMaterial ||
           material instanceof THREE.MeshLambertMaterial ||
           material.lights )
        {
          if ( _lightsNeedUpdate )
            {
              refreshLights = true;
              setupLights( lights );
              _lightsNeedUpdate = false;
            }

          if ( refreshLights )
            {
              refreshUniformsLights( m_uniforms, _lights );
              markUniformsLightsNeedsUpdate( m_uniforms, true );
            }
          else
            {
              markUniformsLightsNeedsUpdate( m_uniforms, false );
            }
        }
#endif

#if TODO
      if (
          GTHREE_IS_BASIC_MATERIAL (material) ||
           || material instanceof THREE.MeshLambertMaterial ||
           material instanceof THREE.MeshPhongMaterial
          )
        {
          refreshUniformsCommon( m_uniforms, material );
        }
#endif

      // refresh single material specific uniforms

#if TODO
      if ( material instanceof THREE.LineBasicMaterial )
        {
          refreshUniformsLine( m_uniforms, material );
        }
      else if ( material instanceof THREE.LineDashedMaterial )
        {
          refreshUniformsLine( m_uniforms, material );
          refreshUniformsDash( m_uniforms, material );
        }
      else if ( material instanceof THREE.PointCloudMaterial )
        {
          refreshUniformsParticle( m_uniforms, material );
        }
      else if ( material instanceof THREE.MeshPhongMaterial )
        {
          refreshUniformsPhong( m_uniforms, material );
        }
      else if ( material instanceof THREE.MeshLambertMaterial )
        {
          refreshUniformsLambert( m_uniforms, material );
        }
      else if ( material instanceof THREE.MeshDepthMaterial )
        {
          m_uniforms.mNear.value = camera.near;
          m_uniforms.mFar.value = camera.far;
          m_uniforms.opacity.value = material.opacity;
        }
      else if ( material instanceof THREE.MeshNormalMaterial )
        {
          m_uniforms.opacity.value = material.opacity;
        }

      if ( object.receiveShadow && ! material._shadowPass )
        {
          refreshUniformsShadow( m_uniforms, lights );
        }
#endif

      // load common uniforms
      gthree_uniforms_load (m_uniforms, renderer);
    }

  load_uniforms_matrices (renderer, program, object);

  location = gthree_program_lookup_uniform_location (program, "modelMatrix");
  if (location >= 0)
    {
      float matrix[16];
      gthree_object_get_world_matrix_floats (object, matrix);
      glUniformMatrix4fv (location, 1, FALSE, matrix);
    }

  return program;
}

static void
init_attributes (GthreeRenderer *renderer)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);
  int i;

  for (i = 0; i < G_N_ELEMENTS(priv->new_attributes); i++)
    priv->new_attributes[i] = 0;
}

static void
enable_attribute (GthreeRenderer *renderer,
                  guint attribute)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);

  priv->new_attributes[attribute] = 1;
  if (priv->enabled_attributes[attribute] == 0)
    {
      glEnableVertexAttribArray(attribute);
      priv->enabled_attributes[attribute] = 1;
    }
}

static void
disable_unused_attributes (GthreeRenderer *renderer)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);
  int i;

  for (i = 0; i < G_N_ELEMENTS(priv->new_attributes); i++)
    {
      if (priv->enabled_attributes[i] != priv->new_attributes[i])
        {
          glDisableVertexAttribArray(i);
          priv->enabled_attributes[i] = 0;
        }
    }
}

static void
render_buffer (GthreeRenderer *renderer,
               GthreeCamera *camera,
               gpointer lights,
               gpointer fog,
               GthreeMaterial *material,
               GthreeBuffer *buffer)
{
  GthreeObject *object = buffer->object;
  GthreeProgram *program = set_program (renderer, camera, lights, fog, material, object);
  //var linewidth, a, attribute, i, il;
  //var attributes = program.attributes;
  gboolean updateBuffers = false;
  gint position_location, color_location, uv_location;
  guint32 wireframeBit = gthree_material_get_is_wireframe (material) ? 1 : 0;
  guint32 geometryGroupHash = (guint32)buffer + (guint32)program * 2 + wireframeBit;

  if (!gthree_material_get_is_visible (material))
    return;

  if (TRUE /* geometryGroupHash !== _currentGeometryGroupHash */)
    {
      //_currentGeometryGroupHash = geometryGroupHash;
      updateBuffers = true;
    }

  if (updateBuffers)
    init_attributes (renderer);

  // vertices
  position_location = gthree_program_lookup_attribute_location (program, "position");
  if (/*!material.morphTargets && */ position_location >= 0)
    {
      if (updateBuffers)
        {
          glBindBuffer (GL_ARRAY_BUFFER, buffer->vertex_buffer);
          enable_attribute (renderer, position_location);
          glVertexAttribPointer (position_location, 3, GL_FLOAT, FALSE, 0, NULL);
        }
    }
  else
    {
      /*
      if (object.morphTargetBase)
        setupMorphTargets( material, geometryGroup, object );
      */
    }

  if (updateBuffers)
    {
      // custom attributes
      // Use the per-geometryGroup custom attribute arrays which are setup in initMeshBuffers
#if TODO
      if (geometryGroup.__webglCustomAttributesList )
        {
          for ( i = 0, il = geometryGroup.__webglCustomAttributesList.length; i < il; i ++ )
            {
              attribute = geometryGroup.__webglCustomAttributesList[ i ];
              if ( attributes[ attribute.buffer.belongsToAttribute ] >= 0 )
                {
                  _gl.bindBuffer( _gl.ARRAY_BUFFER, attribute.buffer );
                  enableAttribute( attributes[ attribute.buffer.belongsToAttribute ] );
                  _gl.vertexAttribPointer( attributes[ attribute.buffer.belongsToAttribute ], attribute.size, _gl.FLOAT, false, 0, 0 );
                }
            }
        }
#endif

      // colors
      color_location = gthree_program_lookup_attribute_location (program, "color");
      if (color_location >= 0)
        {
          if ( TRUE /* object.geometry.colors.length > 0 || object.geometry.faces.length > 0 */ )
            {
              glBindBuffer (GL_ARRAY_BUFFER, buffer->color_buffer);
              enable_attribute (renderer, color_location);
              glVertexAttribPointer (color_location, 3, GL_FLOAT, FALSE, 0, NULL);
            }
#if TODO
          else if (material.defaultAttributeValues)
            {
              glVertexAttrib3fv (attributes.color, material.defaultAttributeValues.color);
            }
#endif
        }

      // uvs
      uv_location = gthree_program_lookup_attribute_location (program, "uv");
      if (uv_location >= 0)
        {
          if ( TRUE /* object.geometry.faceVertexUvs[ 0 ] */ )
            {
              glBindBuffer (GL_ARRAY_BUFFER, buffer->uv_buffer);
              enable_attribute (renderer, uv_location);
              glVertexAttribPointer (uv_location, 2, GL_FLOAT, FALSE, 0, NULL);
            }
#if TODO
          else if (material.defaultAttributeValues)
            {
              glVertexAttrib2fv (attributes.uv, material.defaultAttributeValues.uv);
            }
#endif
        }

#if TODO
      // normals
      if ( attributes.normal >= 0 )
        {
          _gl.bindBuffer( _gl.ARRAY_BUFFER, geometryGroup.__webglNormalBuffer );
          enableAttribute( attributes.normal );
          _gl.vertexAttribPointer( attributes.normal, 3, _gl.FLOAT, false, 0, 0 );
        }

      // skinning

      // line distances
#endif

      disable_unused_attributes (renderer);

      // render mesh
      if (TRUE /* object instanceof THREE.Mesh */ )
        {
          if (gthree_material_get_is_wireframe (material))
            {
              // wireframe
              set_line_width (renderer, gthree_material_get_wireframe_line_width (material));
              if (updateBuffers)
                glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, buffer->line_buffer);
              glDrawElements (GL_LINES, buffer->line_count, GL_UNSIGNED_SHORT, 0 );
            }
          else
            {
              // triangles
              if (updateBuffers)
                glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, buffer->face_buffer);
              glDrawElements (GL_TRIANGLES, buffer->face_count, GL_UNSIGNED_SHORT, 0 );
            }
        }
    }
}

static void
render_objects (GthreeRenderer *renderer,
                GPtrArray *render_list,
                GthreeCamera *camera,
                gpointer lights,
                gpointer fog,
                gboolean use_blending,
                GthreeMaterial *override_material)
{
  GthreeBuffer *buffer;
  GthreeMaterial *material;
  int i;

  for (i = 0; i < render_list->len; i++)
    {
      buffer = g_ptr_array_index (render_list, i);

      gthree_object_update_matrix_view (buffer->object, gthree_camera_get_world_inverse_matrix (camera));

      if (override_material)
        material = override_material;
      else
        material = buffer->material;

      if (material == NULL)
        continue;

      if (use_blending)
        {
          guint equation, src_factor, dst_factor;
          GthreeBlendMode mode = gthree_material_get_blend_mode (material, &equation, &src_factor, &dst_factor);

          set_blending (renderer, mode, equation, src_factor, dst_factor);
        }

      set_depth_test (renderer, gthree_material_get_depth_test (material));
      set_depth_write (renderer, gthree_material_get_depth_write (material));


      {
        gboolean polygon_offset;
        float factor, units;

        polygon_offset = gthree_material_get_polygon_offset (material, &factor, &units);
        set_polygon_offset (renderer, polygon_offset, factor, units);
      }
      set_material_faces (renderer, material);

      render_buffer (renderer, camera, lights, fog, material, buffer);
    }
}

static void
clear (gboolean color, gboolean depth, gboolean stencil)
{
  guint bits = 0;
  if (color)
    bits |= GL_COLOR_BUFFER_BIT;
  if (depth)
    bits |= GL_DEPTH_BUFFER_BIT;
  if (stencil)
    bits |= GL_STENCIL_BUFFER_BIT;

  glClear (bits);
}

void
gthree_renderer_render (GthreeRenderer *renderer,
                        GthreeScene    *scene,
                        GthreeCamera   *camera,
                        gboolean force_clear)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);
  GthreeMaterial *override_material;
  gpointer lights, fog;

  lights = NULL;
  fog = NULL;

  /* update scene graph */

  gthree_object_update_matrix_world (GTHREE_OBJECT (scene), FALSE);

  /* update camera matrices and frustum */

  if (gthree_object_get_parent (GTHREE_OBJECT (camera)) == NULL)
    gthree_object_update_matrix_world (GTHREE_OBJECT (camera), FALSE);

  if (priv->auto_clear)
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  gthree_camera_update_matrix (camera);

  gthree_camera_get_proj_screen_matrix (camera, &priv->proj_screen_matrix);
  gthree_frustum_init_from_matrix (&priv->frustum, &priv->proj_screen_matrix);

  gthree_scene_realize_objects (scene);

  g_ptr_array_set_size (priv->opaque_objects, 0);
  g_ptr_array_set_size (priv->transparent_objects, 0);

  project_object (renderer, scene, GTHREE_OBJECT (scene), camera);

  if (priv->sort_objects)
    {
      g_ptr_array_sort (priv->opaque_objects, painter_sort_stable);
      g_ptr_array_sort (priv->transparent_objects, reverse_painter_sort_stable);
    }

  //this.setRenderTarget( renderTarget );

  if (priv->auto_clear || force_clear )
    {
      clear (priv->auto_clear_color, priv->auto_clear_depth, priv->auto_clear_stencil);
    }

  /* set matrices for regular objects (frustum culled) */


  override_material = gthree_scene_get_override_material (scene);
  if (override_material)
    {
      gboolean polygon_offset;
      float factor, units;
      guint equation, src_factor, dst_factor;
      GthreeBlendMode mode = gthree_material_get_blend_mode (override_material, &equation, &src_factor, &dst_factor);

      set_blending (renderer, mode, equation, src_factor, dst_factor);

      set_depth_test (renderer, gthree_material_get_depth_test (override_material));
      set_depth_write (renderer, gthree_material_get_depth_write (override_material));
      polygon_offset = gthree_material_get_polygon_offset (override_material, &factor, &units);
      set_polygon_offset (renderer, polygon_offset, factor, units);

      render_objects (renderer, priv->opaque_objects, camera, lights, fog, TRUE, override_material );
      render_objects (renderer, priv->transparent_objects, camera, lights, fog, TRUE, override_material );
    }
  else
    {
      // opaque pass (front-to-back order)

      set_blending (renderer, GTHREE_BLEND_NO, 0, 0, 0);
      render_objects (renderer, priv->opaque_objects, camera, lights, fog, FALSE, NULL);

      // transparent pass (back-to-front order)
      render_objects (renderer, priv->transparent_objects, camera, lights, fog, TRUE, NULL);
    }
}

guint
gthree_renderer_allocate_texture_unit (GthreeRenderer *renderer)
{
  GthreeRendererPrivate *priv = gthree_renderer_get_instance_private (renderer);
  guint texture_unit = priv->used_texture_units;

  if (texture_unit >= priv->max_textures )
    g_warning ("Trying to use %dtexture units while this GPU supports only %d",  texture_unit,priv->max_textures);

  priv->used_texture_units += 1;

  return texture_unit;
}
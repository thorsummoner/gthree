#include <math.h>
#include <epoxy/gl.h>

#include "gthreegeometry.h"
#include "gthreegeometry.h"

typedef struct {
  GArray *vertices; /* graphene_vec3_t */

  GArray *colors; /* GdkRGBA, per vertex */

  GPtrArray *faces; /* GthreeFace* */

  GArray *uv; /* graphene_vec2_t */
  GArray *uv2; /* graphene_vec2_t */

  GthreeBox3 bounding_box;
  GthreeSphere bounding_sphere;

  guint bounding_box_set;
  guint bounding_sphere_set;

} GthreeGeometryPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GthreeGeometry, gthree_geometry, G_TYPE_OBJECT);

GthreeGeometry *
gthree_geometry_new ()
{
  GthreeGeometry *geometry;

  geometry = g_object_new (gthree_geometry_get_type (),
                         NULL);

  return geometry;
}

void
gthree_geometry_add_vertex (GthreeGeometry *geometry,
                            graphene_vec3_t *v)
{
  GthreeGeometryPrivate *priv = gthree_geometry_get_instance_private (geometry);

  g_array_append_val (priv->vertices,*v);
}

guint
gthree_geometry_get_n_vertices (GthreeGeometry *geometry)
{
  GthreeGeometryPrivate *priv = gthree_geometry_get_instance_private (geometry);

  return priv->vertices->len;
}

const graphene_vec3_t *
gthree_geometry_get_vertices (GthreeGeometry *geometry)
{
  GthreeGeometryPrivate *priv = gthree_geometry_get_instance_private (geometry);

  return (graphene_vec3_t *)priv->vertices->data;
}

/* Takes ownership */
void
gthree_geometry_add_face (GthreeGeometry *geometry,
                          GthreeFace *face)
{
  GthreeGeometryPrivate *priv = gthree_geometry_get_instance_private (geometry);

  g_ptr_array_add (priv->faces, face);
}

GthreeFace *
gthree_geometry_get_face (GthreeGeometry *geometry,
                          int i)
{
  GthreeGeometryPrivate *priv = gthree_geometry_get_instance_private (geometry);

  return g_ptr_array_index (priv->faces, i);
}

guint
gthree_geometry_get_n_faces (GthreeGeometry *geometry)
{
  GthreeGeometryPrivate *priv = gthree_geometry_get_instance_private (geometry);

  return priv->faces->len;
}

const graphene_vec2_t *
gthree_geometry_get_uvs (GthreeGeometry *geometry)
{
  GthreeGeometryPrivate *priv = gthree_geometry_get_instance_private (geometry);

  return (graphene_vec2_t *)priv->uv->data;
}

guint
gthree_geometry_get_n_uv (GthreeGeometry *geometry)
{
  GthreeGeometryPrivate *priv = gthree_geometry_get_instance_private (geometry);

  return priv->uv->len;
}

void
gthree_geometry_add_uv (GthreeGeometry *geometry,
                        graphene_vec2_t *v)
{
  GthreeGeometryPrivate *priv = gthree_geometry_get_instance_private (geometry);

  g_array_append_val (priv->uv, *v);
}

const graphene_vec2_t *
gthree_geometry_get_uv2s (GthreeGeometry *geometry)
{
  GthreeGeometryPrivate *priv = gthree_geometry_get_instance_private (geometry);

  return (graphene_vec2_t *)priv->uv2->data;
}

guint
gthree_geometry_get_n_uv2 (GthreeGeometry *geometry)
{
  GthreeGeometryPrivate *priv = gthree_geometry_get_instance_private (geometry);

  return priv->uv2->len;
}

void
gthree_geometry_add_uv2 (GthreeGeometry *geometry,
                         graphene_vec2_t *v)
{
  GthreeGeometryPrivate *priv = gthree_geometry_get_instance_private (geometry);

  g_array_append_val (priv->uv2, *v);
}

const GthreeSphere *
gthree_geometry_get_bounding_sphere (GthreeGeometry *geometry)
{
  GthreeGeometryPrivate *priv = gthree_geometry_get_instance_private (geometry);

  if (!priv->bounding_sphere_set)
    {
      gthree_sphere_init_from_points (&priv->bounding_sphere,
                                      (graphene_vec3_t *)(priv->vertices->data),
                                      priv->vertices->len,
                                      NULL);
      priv->bounding_sphere_set = TRUE;
    }

  return &priv->bounding_sphere;
}

static void
gthree_geometry_init (GthreeGeometry *geometry)
{
  GthreeGeometryPrivate *priv = gthree_geometry_get_instance_private (geometry);

  priv->vertices = g_array_new (FALSE, FALSE, sizeof (graphene_vec3_t));
  priv->colors = g_array_new (FALSE, FALSE, sizeof (GdkRGBA));
  priv->faces = g_ptr_array_new_with_free_func ((GDestroyNotify)g_object_unref);
  priv->uv = g_array_new (FALSE, FALSE, sizeof (graphene_vec2_t));
  priv->uv2 = g_array_new (FALSE, FALSE, sizeof (graphene_vec2_t));
}

static void
gthree_geometry_finalize (GObject *obj)
{
  GthreeGeometry *geometry = GTHREE_GEOMETRY (obj);
  GthreeGeometryPrivate *priv = gthree_geometry_get_instance_private (geometry);

  g_array_free (priv->vertices, TRUE);
  g_array_free (priv->colors, TRUE);
  g_ptr_array_free (priv->faces, TRUE);
  g_array_free (priv->uv, TRUE);
  g_array_free (priv->uv2, TRUE);

  G_OBJECT_CLASS (gthree_geometry_parent_class)->finalize (obj);
}

static void
gthree_geometry_class_init (GthreeGeometryClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = gthree_geometry_finalize;
}
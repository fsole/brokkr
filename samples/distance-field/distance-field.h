#pragma once

#include <stdio.h>
#include <stdlib.h> //malloc
#include <math.h> //sqrtf
#include <string.h> //mempcy

#if defined (_cplusplus)
#define API_EXPORT extern "C"
#else
#define API_EXPORT
#endif

typedef unsigned int u32;

typedef struct dfvec3
{
  float x, y, z;
}dfvec3;

typedef struct AABB_t
{
  dfvec3 min;
  dfvec3 max;
}AABB_t;

typedef struct mesh_t
{
  dfvec3* vertex;
  u32 vertexCount;

  u32* index;
  u32 indexCount;

  AABB_t aabb;
}mesh_t;


typedef struct distance_field_t
{
  u32 height;
  u32 width;
  u32 depth;
  dfvec3 aabbMin;
  dfvec3 aabbMax;
  float* data;
}distance_field_t;


//API
API_EXPORT void meshCreate(dfvec3* vertex, u32 vertexCount, u32* index, u32 indexCount, AABB_t aabb, mesh_t** mesh);
API_EXPORT void meshDestroy(mesh_t** mesh);

API_EXPORT void distanceFieldCreate(u32 width, u32 height, u32 depth, mesh_t* mesh, distance_field_t** field);
API_EXPORT void distanceFieldCreate(u32 width, u32 height, u32 depth, float radius, distance_field_t** field);
API_EXPORT void distanceFieldDestroy(distance_field_t** field);
API_EXPORT float distanceFieldGetPixel(distance_field_t* field, u32 x, u32 y, u32 z);
API_EXPORT void distanceFieldSetPixel(distance_field_t* field, u32 x, u32 y, u32 z, float value);
API_EXPORT void distanceFieldPrint(distance_field_t* field);

#define IMPL

#ifdef IMPL

////Minimal dfvec3 API
static float dfvec3Dot(dfvec3 v, dfvec3 u)
{
  return v.x*u.x + v.y*u.y + v.z*u.z;
}

static dfvec3 dfvec3Cross(dfvec3 v0, dfvec3 v1)
{
  dfvec3 result;
  result.x = v0.y * v1.z - v0.z * v1.y;
  result.y = v0.z * v1.x - v0.x * v1.z;
  result.z = v0.x * v1.y - v0.y * v1.x;

  return result;
}

static void dfvec3Normalize(dfvec3* v)
{
  float iLenght = 1.0f / sqrtf(v->x*v->x + v->y*v->y + v->z*v->z);
  v->x *= iLenght;
  v->y *= iLenght;
  v->z *= iLenght;
}

static dfvec3 dfvec3Subtract(dfvec3 v0, dfvec3 v1)
{
  dfvec3 result;
  result.x = v0.x - v1.x;
  result.y = v0.y - v1.y;
  result.z = v0.z - v1.z;

  return result;
}

static dfvec3 dfvec3Add(dfvec3 v0, dfvec3 v1)
{
  dfvec3 result;
  result.x = v0.x + v1.x;
  result.y = v0.y + v1.y;
  result.z = v0.z + v1.z;

  return result;
}

static float dfvec3Lenght(dfvec3 v)
{
  return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
}

static dfvec3 dfvec3Scale(dfvec3 v, float s)
{
  dfvec3 result = { v.x*s, v.y*s, v.z*s };
  return result;
}

static dfvec3 closestPointOnTriangle(dfvec3 p, dfvec3 a, dfvec3 b, dfvec3 c)
{
  dfvec3 ab = dfvec3Subtract(b, a);
  dfvec3 ac = dfvec3Subtract(c, a);
  dfvec3 ap = dfvec3Subtract(p, a);

  float d1 = dfvec3Dot(ab, ap);
  float d2 = dfvec3Dot(ac, ap);

  if (d1 <= 0.0f && d2 < 0.0f)
    return a;

  dfvec3 bp = dfvec3Subtract(p, b);
  float d3 = dfvec3Dot(ab, bp);
  float d4 = dfvec3Dot(ac, bp);
  if (d3 >= 0.0f && d4 <= d3)
    return b;

  float vc = d1*d4 - d3*d2;
  if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
  {
    float v = d1 / (d1 - d3);
    return dfvec3Add(a, dfvec3Scale(ab, v));
  }

  dfvec3 cp = dfvec3Subtract(p, c);
  float d5 = dfvec3Dot(ab, cp);
  float d6 = dfvec3Dot(ac, cp);
  if (d6 >= 0.0f && d5 <= d6)
    return c;

  float vb = d5*d2 - d1*d6;
  if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
  {
    float w = d2 / (d2 - d6);
    return dfvec3Add(a, dfvec3Scale(ac, w));
  }

  float va = d3*d6 - d5*d4;
  if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
  {
    float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
    return dfvec3Add(b, dfvec3Scale(dfvec3Subtract(c, b), w));
  }

  float denom = 1.0f / (va + vb + vc);
  float v = vb * denom;
  float w = vc * denom;

  return dfvec3Add(a, dfvec3Add(dfvec3Scale(ab, v), dfvec3Scale(ac, w)));
}

static float distancePointTriangle(dfvec3 point, dfvec3 a, dfvec3 b, dfvec3 c)
{
  //Find vector from point to closest point in triangle
  dfvec3 v = dfvec3Subtract(closestPointOnTriangle(point, a, b, c), point);

  //Compute sign of the distance (positive if in front, negative if behind)
  dfvec3 normal = dfvec3Cross(dfvec3Subtract(b, a), dfvec3Subtract(c, a));
  float sign = dfvec3Dot(normal, v) < 0.0f ? 1.0f : -1.0f;

  return sign * dfvec3Lenght(v);
}

static float distancePointMesh(dfvec3 point, mesh_t* mesh)
{
  float minDistance = 10000.0f;
  for (u32 i = 0; i<mesh->indexCount; i += 3)
  {
    float d = distancePointTriangle(point, mesh->vertex[mesh->index[i]], mesh->vertex[mesh->index[i + 1]], mesh->vertex[mesh->index[i + 2]]);
    if (fabsf(d) < fabsf(minDistance))
    {
      minDistance = d;
    }
  }

  return minDistance;
}

//TODO: Do this once for all points in the grid
static dfvec3 gridToLocal(u32 x, u32 y, u32 z, u32 gridWidth, u32 gridHeight, u32 gridDepth, dfvec3 aabbMin, dfvec3 aabbMax)
{
  dfvec3 normalized = { x / (gridWidth - 1.0f), y / (gridHeight - 1.0f), z / (gridDepth - 1.0) };
  dfvec3 result = { normalized.x * (aabbMax.x - aabbMin.x) + aabbMin.x,
    normalized.y * (aabbMax.y - aabbMin.y) + aabbMin.y,
    normalized.z * (aabbMax.z - aabbMin.z) + aabbMin.z };

  return result;
}

static void localToGrid(dfvec3 local, u32 gridWidth, u32 gridHeight, u32 gridDepth, dfvec3 aabbMin, dfvec3 aabbMax, u32* x, u32* y, u32* z)
{
  *x = (u32)((local.x - aabbMin.x) / (aabbMax.x - aabbMin.x) * (gridWidth-1) + 0.5f);
  *y = (u32)((local.y - aabbMin.y) / (aabbMax.y - aabbMin.y) * (gridHeight-1) + 0.5f);
  *z = (u32)((local.z - aabbMin.z) / (aabbMax.z - aabbMin.z) * (gridDepth-1) + 0.5f);
}


////API Implementation
void meshCreate(dfvec3* vertex, u32 vertexCount, u32* index, u32 indexCount, AABB_t aabb, mesh_t** mesh)
{
  *mesh = (mesh_t*)malloc(sizeof(mesh_t));

  (*mesh)->vertexCount = vertexCount;

  (*mesh)->vertex = (dfvec3*)malloc(sizeof(dfvec3)*vertexCount);
  memcpy((*mesh)->vertex, vertex, sizeof(dfvec3)*vertexCount);

  (*mesh)->indexCount = indexCount;
  (*mesh)->index = (u32*)malloc(sizeof(u32)*indexCount);
  memcpy((*mesh)->index, index, sizeof(u32)*indexCount);

  (*mesh)->aabb = aabb;
}

void meshDestroy(mesh_t** mesh)
{
  free((*mesh)->vertex);
  free((*mesh)->index);
  free(*mesh);

}

void distanceFieldCreate(u32 width, u32 height, u32 depth, mesh_t* mesh, distance_field_t** field)
{
  *field = (distance_field_t*)malloc(sizeof(distance_field_t));

  (*field)->width = width;
  (*field)->height = height;
  (*field)->depth = depth;
  (*field)->data = (float*)malloc(sizeof(float) * width * height * depth);

  //Compute distances for an area twice as big as the bounding box of the mesh
  dfvec3 aabbMinScaled = dfvec3Scale(mesh->aabb.min, 4.0f);
  dfvec3 aabbMaxScaled = dfvec3Scale(mesh->aabb.max, 4.0f);
  for (u32 z = 0; z<depth; ++z)
  {
    for (u32 y = 0; y<height; ++y)
    {
      for (u32 x = 0; x<width; ++x)
      {
        float distance = distancePointMesh(gridToLocal(x, y, z, width, height, depth, aabbMinScaled, aabbMaxScaled), mesh);
        distanceFieldSetPixel(*field, x, y, z, distance);
      }
    }
  }

  (*field)->aabbMin = aabbMinScaled;
  (*field)->aabbMax = aabbMaxScaled;
}

void distanceFieldCreate(u32 width, u32 height, u32 depth, float radius, distance_field_t** field)
{
  *field = (distance_field_t*)malloc(sizeof(distance_field_t));

  (*field)->width = width;
  (*field)->height = height;
  (*field)->depth = depth;
  (*field)->data = (float*)malloc(sizeof(float) * width * height * depth);

  //Compute distances for an area twice as big as the bounding box of the mesh
  dfvec3 r = { radius, radius, radius };
  dfvec3 c = { 0.0f,0.0f,0.0f };
  dfvec3 aabbMinScaled = dfvec3Scale(r, -4.0f);
  dfvec3 aabbMaxScaled = dfvec3Scale(r, 4.0f);
  for (u32 z = 0; z<depth; ++z)
  {
    for (u32 y = 0; y<height; ++y)
    {
      for (u32 x = 0; x<width; ++x)
      {
       // u32 ux, uy, uz;
        dfvec3 v = gridToLocal(x, y, z, width, height, depth, aabbMinScaled, aabbMaxScaled);
        float distance = dfvec3Lenght(v)-radius;
        //distance = distance < radius ? -distance : distance;
        distanceFieldSetPixel(*field, x, y, z, distance);
      }
    }
  }

  (*field)->aabbMin = aabbMinScaled;
  (*field)->aabbMax = aabbMaxScaled;
}

void distanceFieldDestroy(distance_field_t** field)
{
  free((*field)->data);
  free(*field);
  *field = 0;
}

float distanceFieldGetPixel(distance_field_t* field, u32 x, u32 y, u32 z)
{
  return field->data[z*field->width*field->height + y*field->width + x];
}

void distanceFieldSetPixel(distance_field_t* field, u32 x, u32 y, u32 z, float value)
{
  field->data[z*field->width*field->height + y*field->width + x] = value;
}

void distanceFieldPrint(distance_field_t* field)
{
  for (u32 z = 0; z<field->depth; ++z)
  {
    for (u32 y = 0; y<field->height; ++y)
    {
      for (u32 x = 0; x<field->width; ++x)
      {
        if ((distanceFieldGetPixel(field, x, y, z)) < 0.0f)
        {
          printf(" o ");
        }
        else
        {
          printf(" x ");
        }
      }
      printf("\n");
    }
    printf("\n\n");
  }
}

#endif



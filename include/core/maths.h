/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#ifndef MATH_H
#define MATH_H

#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h> //RAND_MAX


#define PI       3.14159265358979323846
#define PI_2     1.57079632679489661923 

typedef uint8_t       u8;
typedef uint16_t      u16;
typedef uint32_t      u32;

typedef int8_t        s8;
typedef int16_t       s16;
typedef int32_t       s32;

typedef float         f32;
typedef double        f64;

namespace bkk
{
  namespace core
  {
    namespace maths
    {
      /*********************/
      /* Utility functions */
      /*********************/
      template <typename T> inline T degreeToRadian(T angle);
      template <typename T> inline T radianToDegree(T angle);
      template <typename T> inline T minValue(T a, T b);
      template <typename T> inline T maxValue(T a, T b);
      template <typename T> inline T saturate(const T& value);
      template <typename T> inline T clamp(const T& a, const T& b, const T& value);
      template <typename T> inline T lerp(const T& a, const T& b, f32 t);
      template <typename T> inline T cubicInterpolation(const T& p0, const T& p1, const T&  p2, const T&  p3, f32 progress);
      template <typename T> inline T random(T minValue, T maxValue);

      /****************/
      /* Vector2 Base */
      /****************/
      template <typename T, u32 N>
      struct Vector
      {
        Vector<T, N>();
        ~Vector<T, N>();
        T& operator[](u32 n);
        const T& operator[](u32 n) const;
        T data[N];
      };
      
      /***********/
      /* Vector2 */
      /***********/
      template <typename T>
      struct Vector<T, 2>
      {
        Vector<T, 2>();
        Vector<T, 2>(const T a, const T b);
        Vector<T, 2>(const T a);

        ~Vector<T, 2>();

        T& operator[](u32 n);
        const T& operator[](u32 n) const;

        union { T data[2]; struct { T x, y; }; };
      };

      /***********/
      /* Vector3 */
      /***********/
      template <typename T>
      struct Vector<T, 3>
      {
        Vector<T, 3>();
        Vector<T, 3>(const T a, const T b, const T c);
        Vector<T, 3>(const T a);

        ~Vector<T, 3>();

        T& operator[](u32 n);
        const T& operator[](u32 n) const;
        void normalize();

        union
        {
          T data[3];
          struct { T x, y, z; };
          struct { T r, g, b; };
        };
      };

      /***********/
      /* Vector4 */
      /***********/
      template <typename T>
      struct Vector<T, 4>
      {
        Vector<T, 4>();
        Vector<T, 4>(const T a, const T b, const T c, const T d);
        Vector<T, 4>(const Vector<T, 3>& v, T d);
        Vector<T, 4>(const T a);

        ~Vector<T, 4>();

        T& operator[](u32 n);
        const T& operator[](u32 n) const;
        void normalize();

        union
        {
          T data[4];
          struct { T x, y, z, w; };
          struct { T r, g, b, a; };
        };

        Vector<T, 3>& xyz();
        const Vector<T, 3>& xyz() const;
      };

      typedef Vector<f32, 2u> vec2;
      typedef Vector<u32, 2u> uvec2;
      typedef Vector<s32, 2u> ivec2;
      typedef Vector<f32, 3u> vec3;
      typedef Vector<u32, 3u> uvec3;
      typedef Vector<s32, 3u> ivec3;
      typedef Vector<f32, 4u> vec4;
      typedef Vector<u32, 4u> uvec4;
      typedef Vector<u32, 2u> uvec2;
      typedef Vector<u32, 3u> uvec3;
      typedef Vector<u32, 4u> uvec4;

      static const vec3 VEC3_ZERO = vec3(0.0f, 0.0f, 0.0f);
      static const vec3 VEC3_ONE = vec3(1.0f, 1.0f, 1.0f);      
      static const vec3 VEC3_RIGHT = vec3(1.0f, 0.0f, 0.0f);
      static const vec3 VEC3_UP = vec3(0.0f, 1.0f, 0.0f);
      static const vec3 VEC3_FORWARD = vec3(0.0f, 0.0f, 1.0f);


      /********************/
      /* Vector functions */
      /********************/
      template <typename T, u32 N>
      inline Vector<T, N> operator+(const Vector<T, N>& v0, const Vector<T, N>& v1);

      template <typename T, u32 N>
      inline Vector<T, N> operator+=(Vector<T, N>& v0, const Vector<T, N>& v1);

      template <typename T, u32 N>
      inline Vector<T, N> operator-(const Vector<T, N>& v0, const Vector<T, N>& v1);

      template <typename T, u32 N>
      inline Vector<T, N> operator+(const Vector<T, N>& v0, const T n);

      template <typename T, u32 N>
      inline Vector<T, N> operator-(T n, const Vector<T, N>& v1);

      template <typename T, u32 N>
      inline Vector<T, N> operator+(T n, const Vector<T, N>& v1);

      template <typename T, u32 N>
      inline Vector<T, N> negate(const Vector<T, N>& v0);

      template <typename T, u32 N>
      inline Vector<T, N> operator*(const Vector<T, N>& v0, const Vector<T, N>& v1);

      template <typename T, u32 N>
      inline Vector<T, N> operator*(const T a, const Vector<T, N>& v0);

      template <typename T, u32 N>
      inline Vector<T, N> operator*(const Vector<T, N>& v0, const T a);

      template <typename T, u32 N>
      inline Vector<T, N> operator/(const Vector<T, N>& v0, const T a);

      template <typename T, u32 N>
      inline Vector<T, N> operator*=(Vector<T, N>& v0, const T a);

      template <typename T, u32 N>
      inline Vector<T, N> operator/=(Vector<T, N>& v0, const T a);

      template <typename T, u32 N>
      inline bool operator==(const Vector<T, N>& v0, const Vector<T, N>& v1);

      template <typename T, u32 N>
      inline bool operator!=(const Vector<T, N>& v0, const Vector<T, N>& v1);

      template <typename T, u32 N>
      inline T dot(const Vector<T, N>& v0, const Vector<T, N>& v1);

      template <typename T>
      inline Vector<T, 3> cross(const Vector<T, 3>& v0, const Vector<T, 3>& v1);

      template <typename T, u32 N>
      inline f32 lengthSquared(const Vector<T, N>& v);

      template <typename T, u32 N>
      inline f32 length(const Vector<T, N>& v);

      template <typename T, u32 N>
      inline Vector<T, N> normalize(const Vector<T, N>& v);

      template <typename T, u32 N>
      inline Vector<T, N> reflect(const Vector<T, N>& v, const Vector<T, N>& n);

      /**************/
      /* Quaternion */
      /**************/
      template <typename T>
      struct Quaternion
      {
        Quaternion<T>();
        Quaternion(T a, T b, T c, T d);
        Quaternion(const Vector<T, 4>& v);
        Quaternion(const vec3& from, const vec3& to);
        Quaternion(const Vector<T, 3>& axis, T angle);

        ~Quaternion<T>();

        T& operator[](u32 n);
        void normalize();
        Vector<T, 4> asVec4() const;

        union
        {
          T data[4];
          struct { T x, y, z, w; };
        };
      };

      typedef struct Quaternion<f32> quat;
      static const quat QUAT_UNIT = quat(0.0f, 0.0f, 0.0f, 1.0f);
      
      template <typename T>
      inline Quaternion<T> quaternionFromAxisAngle(const Vector<T, 3>& axis, T angle);

      template <typename T>
      inline Quaternion<T> operator*(const Quaternion<T>& q0, const Quaternion<T>& q1);

      template <typename T>
      inline Quaternion<T> operator*(const Quaternion<T>& v0, f32 s);

      template <typename T>
      inline Quaternion<T> operator-(const Quaternion<T>& v0);

      template <typename T>
      inline Quaternion<T> operator+(const Quaternion<T>& v0, const Quaternion<T>& v1);

      template <typename T>
      inline Quaternion<T> operator-(const Quaternion<T>& v0, const Quaternion<T>& v1);

      template <typename T>
      inline Quaternion<T> slerp(const Quaternion<T>& q0, const Quaternion<T>& q1, f32 t);

      template <typename T>
      inline Quaternion<T> conjugate(const Quaternion<T>& q);

      template <typename T>
      inline Vector<T, 4> rotate(const Vector<T, 4>& v, const Quaternion<T>& q);

      template <typename T>
      inline Vector<T, 3> rotate(const Vector<T, 3>& v, const Quaternion<T>& q);
      
      /***************/
      /* Matrix Base */
      /***************/
      template <typename T, u32 ROWS, u32 COLUMNS>
      struct Matrix
      {
        Matrix<T, ROWS, COLUMNS>();
        ~Matrix<T, ROWS, COLUMNS>();
        T& operator[](u32 index);
        T data[ROWS*COLUMNS];
      };

      /***************/
      /* 3x3 Matrix  */
      /***************/
      template <typename T>
      struct Matrix<T, 3, 3>
      {
        Matrix<T, 3, 3>();

        ~Matrix<T, 3, 3>();

        T& operator[](u32 index);
        void setIdentity();
        void setScale(const T sx, const T sy, const T sz);

        union
        {
          T data[9];
          struct {
            T c00, c10, c20,
              c01, c11, c21,
              c02, c12, c22;
          };
        };
      };

      /***************/
      /* 4x4 Matrix  */
      /***************/
      template <typename T>
      struct Matrix<T, 4, 4>
      {
        Matrix<T, 4, 4>();
        Matrix<T, 4, 4>(const Matrix<T, 4, 4>& m);
        Matrix<T, 4, 4>(const T* coefficients);

        ~Matrix<T, 4, 4>();

        void operator=(const Matrix<T, 4, 4>& m);
        void operator=(const T* coefficients);
        T& operator[](u32 index);
        const T& operator[](u32 index) const;
        T& operator()(u8 x, u8 y);
        const T& operator()(u8 x, u8 y) const;

        void setIdentity();
        void setScale(const T sx, const T sy, const T sz);
        void setTranslation(const vec3& translation);
        vec4 getTranslation();
        void transpose();
        Vector<T, 4> row(unsigned int i) const;
        Vector<T, 4> column(unsigned int i) const;

        union
        {
          //Data stored in row major order
          T data[16];
          struct {
            T c00, c10, c20, c30,
              c01, c11, c21, c31,
              c02, c12, c22, c32,
              c03, c13, c23, c33;
          };
        };
      };

      typedef Matrix<f32, 3u, 3u> mat3;
      typedef Matrix<f32, 4u, 4u> mat4;

      /********************/
      /* Matrix Functions */
      /********************/
      template <typename T>
      inline Matrix<T, 4, 4> operator*(const Matrix<T, 4, 4>& m0, const Matrix<T, 4, 4>& m1);
      
      template< typename T>
      inline Vector<T, 4> operator*(const Vector<T, 4>& v, const Matrix<T, 4, 4>& m);

      template< typename T>
      inline Vector<T, 3> operator*(const Vector<T, 3>& v, const Matrix<T, 3, 3>& m);

      template <typename T>
      inline Matrix<T, 4, 4> createTransform(const Vector<T, 3>& translation, const Vector<T, 3>& scale, const Quaternion<T>& rotation);      

      template <typename T>
      inline Matrix<T, 4, 4> invertTransform(const Matrix<T, 4, 4>& m);

      template <typename T>
      inline bool invertMatrix(const Matrix<T, 4, 4>& m, Matrix<T, 4, 4>* result);

      template <typename T>
      inline Matrix<T, 4u, 4u> perspectiveProjectionMatrix(T fov, T aspect, T n, T f);

      template <typename T>
      inline Matrix<T, 4u, 4u> lookAtMatrix(Vector<T, 3> eye, Vector<T, 3> center, Vector<T, 3> up);

      template <typename T>
      inline Matrix<T, 4, 4> orthographicProjectionMatrix(T left, T right, T bottom, T top, T nearPlane, T farPlane);    

      template< typename T>
      static void frustumPlanesFromMatrix(Matrix<T,4,4> matrix, Vector<T,4>* frustumPlanes);

      /*********/
      /* AABB  */
      /*********/
      template< typename T>
      struct AABB
      {
        Vector<T,3> min;
        Vector<T,3> max;
      };

      typedef AABB<f32> aabb_t;

      template<typename T>
      AABB<T> aabbTransform(const AABB<T>& aabb, const Matrix<T,4,4>& transform);

      template< typename T>
      bool aabbInFrustum(const AABB<T>& aabb, Vector<T,4>* frustumPlanes);


      /***********************/
      /***********************/
      /* API Implementation  */
      /***********************/
      /***********************/

      template <typename T>
      inline T degreeToRadian(T angle)
      {
        return T(angle * PI / 180.0);
      }

      template <typename T>
      inline T radianToDegree(T angle)
      {
        return T(angle * 180.0 / PI);
      }

      template <typename T>
      inline T minValue(T a, T b)
      {
        if (a <= b)
          return a;
        else
          return b;
      }

      template <typename T>
      inline T maxValue(T a, T b)
      {
        if (a >= b)
          return a;
        else
          return b;
      }

      template <typename T>
      inline T saturate(const T& value)
      {
        return minValue(maxValue(value, T(0.0)), T(1.0));
      }

      template <typename T>
      inline T clamp(const T& a, const T& b, const T& value)
      {
        return minValue(maxValue(value, a), b);
      }

      template <typename T>
      inline T lerp(const T& a, const T& b, f32 t)
      {
        return a + t * (b - a);
      }

      template <typename T>
      inline T cubicInterpolation(const T& p0, const T& p1, const T&  p2, const T&  p3, f32 progress)
      {
        T a3 = p3 * T(0.5) - p2 * T(1.5) + p1 * T(1.5) - p0 * T(0.5);
        T a2 = p0 - p1 * T(2.5) + p2 * T(2.0) - p3 * T(0.5);
        T a1 = (p2 - p0) * T(0.5);

        return progress * progress*progress*a3 + progress * progress*a2 + progress * a1 + p1;
      }

      template <typename T>
      inline T random(T minValue, T maxValue)
      {
        return (T)((rand() / (RAND_MAX + 1.0)) * (maxValue - minValue) + minValue);
      }

      //// VECTORS

      //Vector base
      template <typename T, u32 N>
      Vector<T, N>::Vector()
      {
        memset(data, 0, N * sizeof(T));
      }

      template <typename T, u32 N>
      Vector<T, N>::~Vector() {}

      template <typename T, u32 N>
      T& Vector<T, N>::operator[](u32 n)
      {
        return data[n];
      }

      template <typename T, u32 N>
      const T& Vector<T, N>::operator[](u32 n) const
      {
        return data[n];
      }

      //Vector2
      template <typename T>
      Vector<T, 2>::Vector() :x(T(0.0)), y(T(0.0)) {}

      template <typename T>
      Vector<T, 2>::Vector(const T a, const T b) : x(a), y(b) {}

      template <typename T>
      Vector<T, 2>::Vector(const T a) : x(a), y(a) {}

      template <typename T>
      Vector<T, 2>::~Vector() {}

      template <typename T>
      T& Vector<T, 2>::operator[](u32 n)
      {
        return data[n];
      }

      template <typename T>
      const T& Vector<T, 2>::operator[](u32 n) const
      {
        return data[n];
      }


      //Vector3
      template <typename T>
      Vector<T, 3>::Vector() : x(T(0.0)), y(T(0.0)), z(T(0.0)) {}

      template <typename T>
      Vector<T, 3>::Vector(const T a, const T b, const T c) : x(a), y(b), z(c) {}

      template <typename T>
      Vector<T, 3>::Vector(const T a) : x(a), y(a), z(a) {}

      template <typename T>
      Vector<T, 3>::~Vector() {}

      template <typename T>
      T& Vector<T, 3>::operator[](u32 n) { return data[n]; }

      template <typename T>
      const T& Vector<T, 3>::operator[](u32 n) const { return data[n]; }

      template <typename T>
      void Vector<T, 3>::normalize()
      {
        f32 inverselength = 1.0f / length(*this);
        x *= inverselength;
        y *= inverselength;
        z *= inverselength;
      }

      //Vector4
      template <typename T>
      Vector<T, 4>::Vector() : x(T(0.0)), y(T(0.0)), z(T(0.0)), w(T(0.0)) {}

      template <typename T>
      Vector<T, 4>::Vector(const T a, const T b, const T c, const T d) : x(a), y(b), z(c), w(d) {}

      template <typename T>
      Vector<T, 4>::Vector(const Vector<T, 3>& v, T d) : x(v.x), y(v.y), z(v.z), w(d) {}

      template <typename T>
      Vector<T, 4>::Vector(const T a) : x(a), y(a), z(a), w(a) {}

      template <typename T>
      Vector<T, 4>::~Vector<T, 4>() {}

      template <typename T>
      T& Vector<T, 4>::operator[](u32 n) { return data[n]; }

      template <typename T>
      const T& Vector<T, 4>::operator[](u32 n) const { return data[n]; }

      template <typename T>
      void Vector<T, 4>::normalize()
      {
        f32 inverselength = 1.0f / length(*this);
        x *= inverselength;
        y *= inverselength;
        z *= inverselength;
        w *= inverselength;
      }

      template <typename T>
      Vector<T, 3>& Vector<T, 4>::xyz() { return reinterpret_cast<Vector<T, 3> &>(data); }

      template <typename T>
      const Vector<T, 3>& Vector<T, 4>::xyz() const { return reinterpret_cast<const Vector<T, 3> &>(data); }
      

      //////Vector functions
      template <typename T, u32 N>
      inline Vector<T, N> operator+(const Vector<T, N>& v0, const Vector<T, N>& v1)
      {
        Vector<T, N> result;
        for (u32 i(0); i < N; ++i)
        {
          result.data[i] = v0.data[i] + v1.data[i];
        }
        return result;
      }

      template <typename T, u32 N>
      inline Vector<T, N> operator+=(Vector<T, N>& v0, const Vector<T, N>& v1)
      {
        for (u32 i(0); i < N; ++i)
        {
          v0.data[i] = v0.data[i] + v1.data[i];
        }
        return v0;
      }

      template <typename T, u32 N>
      inline Vector<T, N> operator-(const Vector<T, N>& v0, const Vector<T, N>& v1)
      {
        Vector<T, N> result;
        for (u32 i(0); i < N; ++i)
        {
          result.data[i] = v0.data[i] - v1.data[i];
        }
        return result;
      }

      template <typename T, u32 N>
      inline Vector<T, N> operator+(const Vector<T, N>& v0, const T n)
      {
        Vector<T, N> result;
        for (u32 i(0); i < N; ++i)
        {
          result.data[i] = v0.data[i] + n;
        }
        return result;
      }

      template <typename T, u32 N>
      inline Vector<T, N> operator-(T n, const Vector<T, N>& v1)
      {
        return v1 + n;
      }

      template <typename T, u32 N>
      inline Vector<T, N> operator+(T n, const Vector<T, N>& v1)
      {
        Vector<T, N> result;
        for (u32 i(0); i < N; ++i)
        {
          result.data[i] = n + v1.data[i];
        }
        return result;
      }

      template <typename T, u32 N>
      inline Vector<T, N> negate(const Vector<T, N>& v0)
      {
        Vector<T, N> result;
        for (u32 i(0); i < N; ++i)
        {
          result.data[i] = -v0.data[i];
        }
        return result;
      }

      template <typename T, u32 N>
      inline Vector<T, N> operator*(const Vector<T, N>& v0, const Vector<T, N>& v1)
      {
        Vector<T, N> result;
        for (u32 i(0); i < N; ++i)
        {
          result.data[i] = v0.data[i] * v1.data[i];
        }
        return result;
      }

      template <typename T, u32 N>
      inline Vector<T, N> operator*(const T a, const Vector<T, N>& v0)
      {
        Vector<T, N> result;
        for (u32 i(0); i < N; ++i)
        {
          result.data[i] = v0.data[i] * a;
        }
        return result;
      }

      template <typename T, u32 N>
      inline Vector<T, N> operator*(const Vector<T, N>& v0, const T a)
      {
        Vector<T, N> result;
        for (u32 i(0); i < N; ++i)
        {
          result.data[i] = v0.data[i] * a;
        }
        return result;
      }

      template <typename T, u32 N>
      inline Vector<T, N> operator/(const Vector<T, N>& v0, const T a)
      {
        Vector<T, N> result;
        for (u32 i(0); i < N; ++i)
        {
          result.data[i] = v0.data[i] / a;
        }
        return result;
      }

      template <typename T, u32 N>
      inline bool operator==(const Vector<T, N>& v0, const Vector<T, N>& v1)
      {
        Vector<T, N> result;
        for (u32 i(0); i < N; ++i)
        {
          if (v0.data[i] != v1.data[i])
            return false;
        }
        return true;
      }

      template <typename T, u32 N>
      inline bool operator!=(const Vector<T, N>& v0, const Vector<T, N>& v1)
      {
        return !(v0 == v1);
      }

      template <typename T, u32 N>
      inline Vector<T, N> operator*=(Vector<T, N>& v0, const T a)
      {
        for (u32 i(0); i < N; ++i)
        {
          v0.data[i] *= a;
        }
        return v0;
      }

      template <typename T, u32 N>
      inline Vector<T, N> operator/=(Vector<T, N>& v0, const T a)
      {
        for (u32 i(0); i < N; ++i)
        {
          v0.data[i] /= a;
        }
        return v0;
      }

      template <typename T, u32 N>
      inline T dot(const Vector<T, N>& v0, const Vector<T, N>& v1)
      {
        T result(0);
        for (u32 i(0); i < N; ++i)
        {
          result += v0.data[i] * v1.data[i];
        }
        return result;
      }

      template <typename T>
      inline Vector<T, 3> cross(const Vector<T, 3>& v0, const Vector<T, 3>& v1)
      {
        Vector<T, 3> result;
        result.x = v0.y * v1.z - v0.z * v1.y;
        result.y = v0.z * v1.x - v0.x * v1.z;
        result.z = v0.x * v1.y - v0.y * v1.x;
        return result;
      }

      template <typename T, u32 N>
      inline f32 lengthSquared(const Vector<T, N>& v)
      {
        f32 lengthSquared(0.0f);
        for (u32 i(0); i < N; ++i)
        {
          lengthSquared += v.data[i] * v.data[i];
        }
        return lengthSquared;
      }

      template <typename T, u32 N>
      inline f32 length(const Vector<T, N>& v)
      {
        return sqrtf(lengthSquared(v));
      }

      template <typename T, u32 N>
      inline Vector<T, N> normalize(const Vector<T, N>& v)
      {
        Vector<T, N> result;
        f32 vlength = length(v);
        if (vlength != 0.0f)
        {
          const f32 inverselength = 1.0f / vlength;
          for (u32 i(0); i < N; ++i)
          {
            result[i] = v.data[i] * inverselength;
          }
        }
        return result;
      }

      template <typename T, u32 N>
      inline Vector<T, N> reflect(const Vector<T, N>& v, const Vector<T, N>& n)
      {
        return v - 2.0f * dot(v, n) * n;
      }

      

      ////// QUATERNION
      
      template <typename T>
      Quaternion<T>::Quaternion() : x(T(0.0)), y(T(0.0)), z(T(0.0)), w(T(1.0)) {}

      template <typename T>
      Quaternion<T>::Quaternion(T a, T b, T c, T d) :x(a), y(b), z(c), w(d) {}

      template <typename T>
      Quaternion<T>::Quaternion(const Vector<T, 4>& v)
        :x(v.x), y(v.y), z(v.z), w(v.w)
      {}

      template <typename T>
      Quaternion<T>::Quaternion(const vec3& from, const vec3& to)
      {
        f32 dotProduct = dot(from, to);
        if (dotProduct > 1.0f)
        {
          x = y = z = 0.0f;
          w = 1.0f;
        }
        else if (dotProduct < -1.0f)
        {
          x = y = w = 0.0f;
          z = 1.0f;
        }
        else
        {
          Vector<T, 3> crossProduct = cross(from, to);
          w = 1.0f + dotProduct;
          x = crossProduct.x;
          y = crossProduct.y;
          z = crossProduct.z;
          normalize();
        }
      }

      template <typename T>
      Quaternion<T>::Quaternion(const Vector<T, 3>& axis, T angle)
      {
        Vector<T, 3> axisNormalized = axis;
        axisNormalized.normalize();

        const f32 halfAngle = -angle * 0.5f;
        const f32 halfAngleSin = sinf(halfAngle);

        x = axisNormalized.x * halfAngleSin;
        y = axisNormalized.y * halfAngleSin;
        z = axisNormalized.z * halfAngleSin;
        w = cosf(halfAngle);
      }

      template <typename T>
      Quaternion<T>::~Quaternion() {}

      template <typename T>
      T& Quaternion<T>::operator[](u32 n)
      {
        return data[n];
      }

      template <typename T>
      void Quaternion<T>::normalize()
      {
        f32 length = sqrtf(x*x + y * y + z * z + w * w);
        x /= length;
        y /= length;
        z /= length;
        w /= length;
      }

      template <typename T>
      Vector<T, 4> Quaternion<T>::asVec4() const
      {
        return vec4(x, y, z, w);
      }

      //////Quaternion functions

      ////Counterclockwise rotation around the axis
      template <typename T>
      inline Quaternion<T> quaternionFromAxisAngle(const Vector<T, 3>& axis, T angle)
      {
        return Quaternion<T>(axis, angle);
      }

      //Quaternion composition.
      //Rotating a vector by the product of q0 * q1 is the same as applying q0 first and then q1
      template <typename T>
      inline Quaternion<T> operator*(const Quaternion<T>& v0, const Quaternion<T>& v1)
      {
        Quaternion<T> result;
        result.x = v1.y * v0.z - v1.z * v0.y + v1.w * v0.x + v1.x * v0.w;
        result.y = v1.z * v0.x - v1.x * v0.z + v1.w * v0.y + v1.y * v0.w;
        result.z = v1.x * v0.y - v1.y * v0.x + v1.w * v0.z + v1.z * v0.w;
        result.w = v1.w * v0.w - v1.x * v0.x - v1.y * v0.y - v1.z * v0.z;

        return result;
      }

      template <typename T>
      inline Quaternion<T> operator*(const Quaternion<T>& v0, f32 s)
      {
        Quaternion<T> result;

        result.x = v0.x * s;
        result.y = v0.y * s;
        result.z = v0.z * s;
        result.w = v0.w * s;

        return result;
      }

      template <typename T>
      inline Quaternion<T> operator-(const Quaternion<T>& v0)
      {
        Quaternion<T> result;

        result.x = -v0.x;
        result.y = -v0.y;
        result.z = -v0.z;
        result.w = -v0.w;

        return result;
      }

      template <typename T>
      inline Quaternion<T> operator+(const Quaternion<T>& v0, const Quaternion<T>& v1)
      {
        Quaternion<T> result;

        result.x = v0.x + v1.x;
        result.y = v0.y + v1.y;
        result.z = v0.z + v1.z;
        result.w = v0.w + v1.w;

        return result;
      }

      template <typename T>
      inline Quaternion<T> operator-(const Quaternion<T>& v0, const Quaternion<T>& v1)
      {
        Quaternion<T> result;

        result.x = v0.x - v1.x;
        result.y = v0.y - v1.y;
        result.z = v0.z - v1.z;
        result.w = v0.w - v1.w;

        return result;
      }

      template <typename T>
      inline Quaternion<T> slerp(const Quaternion<T>& q0, const Quaternion<T>& q1, f32 t)
      {
        Quaternion<T> q2;
        float cosTheta = dot(q0.asVec4(), q1.asVec4());
        if (cosTheta < 0.0f)
        {
          cosTheta = -cosTheta;
          q2 = -q1;
        }
        else
        {
          q2 = q1;
        }

        Quaternion<T> result;
        if (fabsf(cosTheta) < 0.95f)
        {
          float sine = sqrtf(1.0f - cosTheta * cosTheta);
          float angle = atan2f(sine, cosTheta);
          float invSine = 1.0f / sine;
          float coeff0 = sinf((1.0f - t) * angle) * invSine;
          float coeff1 = sinf(t * angle) * invSine;

          result = q1 * coeff0 + q2 * coeff1;
        }
        else
        {
          // If the angle is small, use linear interpolation
          result = q0 * (1.0f - t) + q2 * t;
        }

        result.normalize();
        return result;
      }

      template <typename T>
      inline Quaternion<T> conjugate(const Quaternion<T>& q)
      {
        return Quaternion<T>(-q.x, -q.y, -q.z, q.w);
      }

      template <typename T>
      inline Vector<T, 4> rotate(const Vector<T, 4>& v, const Quaternion<T>& q)
      {
        Quaternion<T> qConjugate = conjugate(q);
        Quaternion<T> result = q * Quaternion<T>(v.x, v.y, v.z, 0.0) * conjugate;
        return Vector<T, 4>(result.x, result.y, result.z, result.w);
      }

      template <typename T>
      inline Vector<T, 3> rotate(const Vector<T, 3>& v, const Quaternion<T>& q)
      {
        Quaternion<T> qConjugate = conjugate(q);
        Quaternion<T> result = q * Quaternion<T>(v.x, v.y, v.z, 0.0) * qConjugate;
        return Vector<T, 3>(result.x, result.y, result.z);
      }

      ///// MATRIX
      template <typename T, u32 ROWS, u32 COLUMNS>
      Matrix<T, ROWS, COLUMNS>::Matrix()
      {
        memset(data, 0, ROWS*COLUMNS * sizeof(T));
      }

      template <typename T, u32 ROWS, u32 COLUMNS>
      Matrix<T, ROWS, COLUMNS>::~Matrix() {}

      template <typename T, u32 ROWS, u32 COLUMNS>
      T& Matrix<T, ROWS, COLUMNS>::operator[](u32 index)
      {
        return data[index];
      }
      

      template <typename T>
      Matrix<T, 3, 3>::Matrix()
      {
        setIdentity();
      }

      template <typename T>
      Matrix<T, 3, 3>::~Matrix() {}

      template <typename T>
      T& Matrix<T, 3, 3>::operator[](u32 index)
      {
        return data[index];
      }

      template <typename T>
      void Matrix<T, 3, 3>::setIdentity()
      {
        memset(data, 0, 9 * sizeof(T));
        c00 = c11 = c22 = 1.0f;
      }

      template <typename T>
      void Matrix<T, 3, 3>::setScale(const T sx, const T sy, const T sz)
      {
        memset(data, 0, 9 * sizeof(T));
        c00 = sx;
        c11 = sy;
        c22 = sz;
      }

      template <typename T>
      Matrix<T, 4, 4>::Matrix()
      {
        setIdentity();
      }

      template <typename T>
      Matrix<T, 4, 4>::Matrix (const Matrix<T, 4, 4>& m)
      {
        memcpy(data, m.data, 16 * sizeof(T));
      }

      template <typename T>
      Matrix<T, 4, 4>::Matrix(const T* coefficients)
      {
        memcpy(data, coefficients, 16 * sizeof(T));
      }

      template <typename T>
      Matrix<T, 4, 4>::~Matrix() {}

      template <typename T>
      void Matrix<T, 4, 4>::operator=(const Matrix<T, 4, 4>& m)
      {
        memcpy(data, m.data, 16 * sizeof(T));
      }

      template <typename T>
      void Matrix<T, 4, 4>::operator=(const T* coefficients)
      {
        coefficients ?
          memcpy(data, coefficients, 16 * sizeof(T)) :
          memset(data, 0, 16 * sizeof(T));
      }

      template <typename T>
      T& Matrix<T, 4, 4>::operator[](u32 index)
      {
        return data[index];
      }

      template <typename T>
      const T& Matrix<T, 4, 4>::operator[](u32 index) const
      {
        return data[index];
      }

      template <typename T>
      T& Matrix<T, 4, 4>::operator()(u8 x, u8 y)
      {
        return data[x * 4 + y];
      }

      template <typename T>
      const T& Matrix<T, 4, 4>::operator()(u8 x, u8 y) const
      {
        return data[x * 4 + y];
      }

      template <typename T>
      void Matrix<T, 4, 4>::setIdentity()
      {
        memset(data, 0, 16 * sizeof(T));
        c00 = c11 = c22 = c33 = 1.0f;
      }

      template <typename T>
      void Matrix<T, 4, 4>::setScale(const T sx, const T sy, const T sz)
      {
        memset(data, 0, 9 * sizeof(T));
        c00 = sx;
        c11 = sy;
        c22 = sz;
      }

      template <typename T>
      void Matrix<T, 4, 4>::setTranslation(const vec3& translation)
      {
        data[12] = translation.x;
        data[13] = translation.y;
        data[14] = translation.z;
      }

      template <typename T>
      vec4 Matrix<T, 4, 4>::getTranslation()
      {
        return vec4(data[12], data[13], data[14], 1.0);
      }

      template <typename T>
      void Matrix<T, 4, 4>::transpose()
      {
        Matrix<T, 4, 4> aux = *this;
        for (u8 i = 0; i < 4; ++i)
        {
          for (u8 j = 0; j < 4; ++j)
          {
            data[i + j * 4] = aux[j + i * 4];
          }
        }
      }

      template <typename T>
      Vector<T, 4> Matrix<T, 4, 4>::row(unsigned int i) const
      {
        return Vector<T, 4>(data[4 * i], data[4 * i + 1], data[4 * i + 2], data[4 * i + 3]);
      }

      template <typename T>
      Vector<T, 4> Matrix<T, 4, 4>::column(unsigned int i) const
      {
        return Vector<T, 4>(data[i], data[i+4], data[i+8], data[i+12]);
      }


      template <typename T>
      inline Matrix<T, 4, 4> operator*(const Matrix<T, 4, 4>& m0, const Matrix<T, 4, 4>& m1)
      {
        Matrix<T, 4, 4> result;
        for (u8 i(0); i < 4; ++i)
        {
          for (u8 j(0); j < 4; ++j)
          {
            result(i, j) = m0(i, 0) * m1(0, j) +
              m0(i, 1) * m1(1, j) +
              m0(i, 2) * m1(2, j) +
              m0(i, 3) * m1(3, j);
          }
        }

        return result;
      }

      template <typename T>
      inline Matrix<T, 4, 4> createTransform(const Vector<T, 3>& translation, const Vector<T, 3>& scale, const Quaternion<T>& rotation)
      {
        Matrix<T, 4, 4> result;

        const f32 xx = rotation.x * rotation.x;
        const f32 yy = rotation.y * rotation.y;
        const f32 zz = rotation.z * rotation.z;
        const f32 xy = rotation.x * rotation.y;
        const f32 xz = rotation.x * rotation.z;
        const f32 xw = rotation.x * rotation.w;
        const f32 yz = rotation.y * rotation.z;
        const f32 yw = rotation.y * rotation.w;
        const f32 zw = rotation.z * rotation.w;


        result[0] = (scale.x * (1.0f - 2.0f * (yy + zz)));
        result[1] = (scale.x * (2.0f * (xy + zw)));
        result[2] = (scale.x * (2.0f * (xz - yw)));
        result[3] = 0.0f;

        result[4] = (scale.y * (2.0f * (xy - zw)));
        result[5] = (scale.y * (1.0f - 2.0f * (xx + zz)));
        result[6] = (scale.y * (2.0f * (yz + xw)));
        result[7] = 0.0f;

        result[8] = (scale.z * (2.0f * (xz + yw)));
        result[9] = (scale.z * (2.0f * (yz - xw)));
        result[10] = (scale.z * (1.0f - 2.0f * (xx + yy)));
        result[11] = 0.0f;

        result[12] = translation.x;
        result[13] = translation.y;
        result[14] = translation.z;
        result[15] = 1.0f;

        return result;
      }

      //Inverse of a transform matrix
      template <typename T>
      inline Matrix<T, 4, 4> invertTransform(const Matrix<T, 4, 4>& m)
      {
        Matrix<T, 4, 4> result;

        result[0] = m[0];
        result[1] = m[4];
        result[2] = m[8];
        result[3] = 0.0f;

        result[4] = m[1];
        result[5] = m[5];
        result[6] = m[9];
        result[7] = 0.0f;

        result[8] = m[2];
        result[9] = m[6];
        result[10] = m[10];
        result[11] = 0.0f;


        result[12] = -((m[0] * m[12]) + (m[1] * m[13]) + (m[2] * m[14]));
        result[13] = -((m[4] * m[12]) + (m[5] * m[13]) + (m[6] * m[14]));
        result[14] = -((m[8] * m[12]) + (m[9] * m[13]) + (m[10] * m[14]));
        result[15] = 1.0f;


        return result;
      }

      //Inverse of a matrix
      template <typename T>
      inline bool invertMatrix(const Matrix<T, 4, 4>& m, Matrix<T, 4, 4>* result)
      {

        result->data[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
        result->data[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
        result->data[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
        result->data[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
        result->data[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
        result->data[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
        result->data[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
        result->data[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
        result->data[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
        result->data[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
        result->data[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
        result->data[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
        result->data[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
        result->data[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
        result->data[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
        result->data[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

        f32 determinant = m[0] * result->data[0] + m[1] * result->data[4] + m[2] * result->data[8] + m[3] * result->data[12];
        if (determinant != 0.0f)
        {
          determinant = 1.0f / determinant;

          for (int i = 0; i < 16; i++)
          {
            result->data[i] *= determinant;
          }
          return true;
        }

        return false;
      }

      template <typename T>
      inline Matrix<T, 4u, 4u> perspectiveProjectionMatrix(T fov, T aspect, T n, T f)
      {
        Matrix<T, 4u, 4u> result;
        memset(result.data, 0, 16 * sizeof(T));

        T height = T(tan(fov * T(0.5f))*n);
        T width = height * aspect;

        result[0] = n / width;
        result[5] = -n / height;
        result[10] = -(f + n) / (f - n);
        result[11] = -1.0f;
        result[14] = (-2.0f*f*n) / (f - n);

        return result;
      }

      template <typename T>
      inline Matrix<T, 4u, 4u> lookAtMatrix(Vector<T, 3> eye, Vector<T, 3> center, Vector<T, 3> up)
      {
        Vector<T, 3> view = normalize(eye - center);
        Vector<T, 3> right = normalize(cross(up, view));
        up = normalize(cross(view, right));

        Matrix<T, 4u, 4u> cameraTx;
        cameraTx[0] = right.x;
        cameraTx[1] = right.y;
        cameraTx[2] = right.z;
        cameraTx[3] = 0.0f;

        cameraTx[4] = up.x;
        cameraTx[5] = up.y;
        cameraTx[6] = up.z;
        cameraTx[7] = 0.0f;

        cameraTx[8] = view.x;
        cameraTx[9] = view.y;
        cameraTx[10] = view.z;
        cameraTx[11] = 0.0f;

        cameraTx[12] = eye.x;
        cameraTx[13] = eye.y;
        cameraTx[14] = eye.z;
        cameraTx[15] = 1.0f;

        return invertTransform(cameraTx);
      }

      template <typename T>
      inline Matrix<T, 4, 4> orthographicProjectionMatrix(T left, T right, T bottom, T top, T nearPlane, T farPlane)
      {
        Matrix<T, 4, 4> result;

        T deltaX = (right - left);
        T deltaY = (top - bottom);
        T deltaZ = (farPlane - nearPlane);


        result[0] = T(2.0 / deltaX);
        result[1] = T(0.0);
        result[2] = T(0.0);
        result[3] = T(-(right + left) / deltaX);

        result[4] = T(0.0);
        result[5] = T(2.0 / deltaY);
        result[6] = T(0.0);
        result[7] = T(-(top + bottom) / deltaY);

        result[8] = T(0.0);
        result[9] = T(0.0);
        result[10] = T(-2.0 / deltaZ);
        result[11] = T(-(farPlane + nearPlane) / deltaZ);

        result[12] = T(0.0);
        result[13] = T(0.0);
        result[14] = T(0.0);
        result[15] = T(1.0);

        return result;
      }


      template< typename T>
      inline Vector<T, 4> operator*(const Vector<T, 4>& v, const Matrix<T, 4, 4>& m)
      {
        Vector<T, 4> result;
        result.x = dot(v, vec4(m.c00, m.c01, m.c02, m.c03));
        result.y = dot(v, vec4(m.c10, m.c11, m.c12, m.c13));
        result.z = dot(v, vec4(m.c20, m.c21, m.c22, m.c23));
        result.w = dot(v, vec4(m.c30, m.c31, m.c32, m.c33));

        return result;
      }

      template< typename T>
      inline Vector<T, 3> operator*(const Vector<T, 3>& v, const Matrix<T, 3, 3>& m)
      {
        Vector<T, 3> result;
        result.x = dot(v, vec3(m.c00, m.c01, m.c02));
        result.y = dot(v, vec3(m.c10, m.c11, m.c12));
        result.z = dot(v, vec3(m.c20, m.c21, m.c22));

        return result;
      }

      typedef Matrix<f32, 3u, 3u> mat3;
      typedef Matrix<f32, 4u, 4u> mat4;

      //Frustum and AABB
      template <typename T>
      void frustumPlanesFromMatrix(Matrix<T,4,4> matrix, Vector<T,4>* frustumPlanes)
      {
        frustumPlanes[0] = matrix.column(3) + matrix.column(0);  //Left
        frustumPlanes[1] = matrix.column(3) - matrix.column(0);  //Right
        frustumPlanes[2] = matrix.column(3) + matrix.column(1);  //Bottom
        frustumPlanes[3] = matrix.column(3) - matrix.column(1);  //Top
        frustumPlanes[4] = matrix.column(3) + matrix.column(2);  //Near
        frustumPlanes[5] = matrix.column(3) - matrix.column(2);  //Far

        //Normalization
        for (uint32_t i(0); i < 6; ++i)
          frustumPlanes[i] /= length(frustumPlanes[i].xyz());;
      }     

      template <typename T>
      AABB<T> aabbTransform(const AABB<T>& aabb, const Matrix<T,4,4>& transform)
      {
        Vector<T, 4> min = vec4(aabb.min, 1.0f) * transform;
        Vector<T, 4> max = vec4(aabb.max, 1.0f) * transform;
        return AABB<T>{ min.xyz(), max.xyz() };
      }

      template <typename T>
      bool aabbInFrustum(const AABB<T>& aabb, Vector<T,4>* frustumPlanes)
      {
        uint32_t out = 0;
        for (uint32_t i = 0; i<6; i++)
        {
          out =  ((dot(frustumPlanes[i], Vector<T, 4>(aabb.min.x, aabb.min.y, aabb.min.z, 1.0f)) < 0.0f) ? 1 : 0);
          out += ((dot(frustumPlanes[i], Vector<T, 4>(aabb.min.x, aabb.min.y, aabb.max.z, 1.0f)) < 0.0f) ? 1 : 0);
          out += ((dot(frustumPlanes[i], Vector<T, 4>(aabb.min.x, aabb.max.y, aabb.min.z, 1.0f)) < 0.0f) ? 1 : 0);
          out += ((dot(frustumPlanes[i], Vector<T, 4>(aabb.min.x, aabb.max.y, aabb.max.z, 1.0f)) < 0.0f) ? 1 : 0);
          out += ((dot(frustumPlanes[i], Vector<T, 4>(aabb.max.x, aabb.min.y, aabb.min.z, 1.0f)) < 0.0f) ? 1 : 0);
          out += ((dot(frustumPlanes[i], Vector<T, 4>(aabb.max.x, aabb.min.y, aabb.max.z, 1.0f)) < 0.0f) ? 1 : 0);
          out += ((dot(frustumPlanes[i], Vector<T, 4>(aabb.max.x, aabb.max.y, aabb.min.z, 1.0f)) < 0.0f) ? 1 : 0);
          out += ((dot(frustumPlanes[i], Vector<T, 4>(aabb.max.x, aabb.max.y, aabb.max.z, 1.0f)) < 0.0f) ? 1 : 0);
          
          //If all the points of the aabb are on the wrong half-space return false
          if (out == 8) 
            return false;
        }

        return true;
      }

    }//math
  } //core
}//bkk
#endif  /*  MATH_H */
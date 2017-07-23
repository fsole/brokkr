#ifndef MATH_H
#define MATH_H

#include <string.h>
#ifdef WIN32
#define NOMINMAX
#define _USE_MATH_DEFINES

#endif
#include <math.h>
#include <iostream>


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
	namespace maths
	{

		template <typename T>
		inline T degreeToRadian(T angle)
		{
			return T(angle * M_PI / 180.0);
		}

		template <typename T>
		inline T radianToDegree(T angle)
		{
			return T(angle * 180.0 / M_PI);
		}

		template <typename T>
		inline T myMin(T a, T b)
		{
      if (a <= b)
        return a; 
      else
        return b;
		}
		
		template <typename T>
		inline T myMax(T a, T b)
		{
      if (a >= b)
        return a;
      else
        return b;
		}

    template <typename T>
    inline T saturate(const T& value)
    {
      return min(max(value, T(0.0)), T(1.0));
    }

    template <typename T>
    inline T clamp(const T& minValue, const T& maxValue, const T& value)
    {
      return min(max(value, minValue), maxValue);
    }

		template <typename T>
		inline T lerp(const T& a, const T& b, f32 t)
		{
			return a + t * (b - a);
		}

		//// VECTORS

		//Vector base
		template <typename T, u32 N>
		struct Vector
		{
			Vector<T, N>()
			{
				memset(data, 0, N * sizeof(T));
			}

			~Vector<T, N>() {}

			T& operator[](u32 n)
			{
				return data[n];
			}

			const T& operator[](u32 n) const
			{
				return data[n];
			}

			T data[N];
		};

		//Vector2
		template <typename T>
		struct Vector<T, 2>
		{
			//Constructors
			Vector<T, 2>() : x(T(0.0)), y(T(0.0)) {}
			Vector<T, 2>(const T a, const T b) : x(a), y(b) {}

			//Destructor
			~Vector<T, 2>() {}

			T& operator[](u32 n)
			{
				return data[n];
			}

			const T& operator[](u32 n) const
			{
				return data[n];
			}

			union { T data[2]; struct { T x, y; }; };
		};

		//Vector3
		template <typename T>
		struct Vector<T, 3>
		{
			Vector<T, 3>() : x(T(0.0)), y(T(0.0)), z(T(0.0)) {}
			Vector<T, 3>(const T a, const T b, const T c) : x(a), y(b), z(c) {}

			~Vector<T, 3>() {}

			T& operator[](u32 n) { return data[n]; }
			const T& operator[](u32 n) const { return data[n]; }

			void normalize()
			{
				f32 inverseLenght = 1.0f / Lenght(*this);
				x *= inverseLenght;
				y *= inverseLenght;
				z *= inverseLenght;
			}

			union
			{
				T data[3];
				struct { T x, y, z; };
				struct { T r, g, b; };
			};
		};

		//Vector4
		template <typename T>
		struct Vector<T, 4>
		{
			Vector<T, 4>() : x(T(0.0)), y(T(0.0)), z(T(0.0)), w(T(0.0)) {}
			Vector<T, 4>(const T a, const T b, const T c, const T d) : x(a), y(b), z(c), w(d) {}
			Vector<T, 4>(const Vector<T, 3>& v, T d) : x(v.x), y(v.y), z(v.z), w(d) {}
			~Vector<T, 4>() {}

			T& operator[](u32 n) { return data[n]; }
			const T& operator[](u32 n) const { return data[n]; }

			void normalize()
			{
				f32 inverseLenght = 1.0f / Lenght(*this);
				x *= inverseLenght;
				y *= inverseLenght;
				z *= inverseLenght;
				w *= inverseLenght;
			}

			union
			{
				T data[4];
				struct { T x, y, z, w; };
				struct { T r, g, b, a; };
			};

			Vector<T, 3>& xyz() { return reinterpret_cast<Vector<T, 3> &>(data); }
			const Vector<T, 3>& xyz() const { return reinterpret_cast<const Vector<T, 3> &>(data); }
		};

		//////Vector functions

		//Addition and sustraction
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

		//Add scalar
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

		//Component-wise multiplication
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

		//Multiplication by a scalar
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

		//Multiplication by a scalar
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

		//Division by a scalar
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
		inline Vector<T, N> operator*=(Vector<T, N>& v0, const T a)
		{
			for (u32 i(0); i < N; ++i)
			{
				v0.data[i] *= a;
			}
			return v0;
		}

		//Dot product
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

		//Cross product. Only for 3-component vectors
		template <typename T>
		inline Vector<T, 3> cross(const Vector<T, 3>& v0, const Vector<T, 3>& v1)
		{
			Vector<T, 3> result;
			result.x = v0.y * v1.z - v0.z * v1.y;
			result.y = v0.z * v1.x - v0.x * v1.z;
			result.z = v0.x * v1.y - v0.y * v1.x;
			return result;
		}

		//LenghtSquared
		template <typename T, u32 N>
		inline f32 lenghtSquared(const Vector<T, N>& v)
		{
			f32 lenghtSquared(0.0f);
			for (u32 i(0); i < N; ++i)
			{
				lenghtSquared += v.data[i] * v.data[i];
			}
			return lenghtSquared;
		}

		//Lenght
		template <typename T, u32 N>
		inline f32 lenght(const Vector<T, N>& v)
		{
			return sqrtf(lenghtSquared(v));
		}

		//Normalization
		template <typename T, u32 N>
		inline Vector<T, N> normalize(const Vector<T, N>& v)
		{
			Vector<T, N> result;
			f32 vLenght = lenght(v);
			if (vLenght != 0.0f)
			{
				const f32 inverseLenght = 1.0f / vLenght;
				for (u32 i(0); i < N; ++i)
				{
					result[i] = v.data[i] * inverseLenght;
				}
			}
			return result;
		}

		//Reflect
		template <typename T, u32 N>
		inline Vector<T, N> reflect(const Vector<T, N>& v, const Vector<T, N>& n)
		{
			return v - 2.0f * Dot(v, n) * n;
		}

		template <typename T, unsigned int N>
		inline Vector<T, N> cubicInterpolation(const Vector<T, N>& p0, const Vector<T, N>& p1, const Vector<T, N>&  p2, const Vector<T, N>&  p3, float progress)
		{
			Vector<T, N> a3 = 0.5f*p3 - 1.5f*p2 + 1.5f*p1 - 0.5f*p0;
			Vector<T, N> a2 = p0 - 2.5f*p1 + 2.0f*p2 - 0.5f*p3;
			Vector<T, N> a1 = 0.5f*(p2 - p0);

			return progress*progress*progress*a3 + progress*progress*a2 + progress*a1 + p1;
		}

		//Print
		template <typename T, u32 N>
		inline std::ostream& operator<<(std::ostream& o, const Vector<T, N>& v)
		{
			o << "[";
			for (u32 i(0); i < N; ++i)
			{
				o << v.data[i];
				if (i != N - 1)
					o << ",";
			}
			o << "]";
			return o;
		}

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

		////// QUATERNION
		template <typename T>
		struct Quaternion
		{
			Quaternion<T>() : x(T(0.0)), y(T(0.0)), z(T(0.0)), w(T(1.0)) {}
			Quaternion(T a, T b, T c, T d) :x(a), y(b), z(c), w(d) {}
			Quaternion(const Vector<T, 4>& v)
				:x(v.x), y(v.y), z(v.z), w(v.w)
			{}

			Quaternion(const vec3& v0, const vec3& v1)
			{
				f32 dot = Dot(v0, v1);
				if (dot > 1.0f)
				{
					x = y = z = 0.0f;
					w = 1.0f;
				}
				else if (dot < -1.0f)
				{
					x = y = w = 0.0f;
					z = 1.0f;
				}
				else
				{
					Vector<T, 3> cross = Cross(v0, v1);
					w = 1.0f + dot;
					x = cross.x;
					y = cross.y;
					z = cross.z;
					Normalize();
				}
			}


			Quaternion(const Vector<T, 3>& axis, T angle)
			{
				Vector<T, 3> axisNormalized = axis;
				axisNormalized.Normalize();

				const f32 halfAngle = angle * 0.5f;
				const f32 halfAngleSin = sinf(halfAngle);

				x = axisNormalized.x * halfAngleSin;
				y = axisNormalized.y * halfAngleSin;
				z = axisNormalized.z * halfAngleSin;
				w = cosf(halfAngle);
			}

			~Quaternion<T>() {}

			T& operator[](u32 n)
			{
				return data[n];
			}

			void normalize()
			{
				f32 length = sqrtf(x*x + y*y + z*z + w*w);
				x /= length;
				y /= length;
				z /= length;
				w /= length;
			}

			Vector<T, 4> AsVec4() const
			{
				return vec4(x, y, z, w);
			}

			union
			{
				T data[4];
				struct { T x, y, z, w; };
			};
		};

		//////Quaternion functions

		//Counter clock-wise rotation around the axis
		template <typename T>
		inline Quaternion<T> quaternionFromAxisAngle(const Vector<T, 3>& axis, T angle)
		{
			Quaternion<T> result;
			Vector<T, 3> axisNormalized = normalize(axis);
			const f32 halfAngle = angle * 0.5f;
			const f32 halfAngleSin = sinf(halfAngle);

			result.x = axisNormalized.x * halfAngleSin;
			result.y = axisNormalized.y * halfAngleSin;
			result.z = axisNormalized.z * halfAngleSin;
			result.w = cosf(halfAngle);

			return result;
		}

		template <typename T>
		inline Quaternion<T> operator*=(Quaternion<T>& v0, const Quaternion<T>& v1)
		{
			Quaternion<T> result;

			result.x = v0.y * v1.z - v0.z * v1.y + v0.w * v1.x + v0.x * v1.w;
			result.y = v0.z * v1.x - v0.x * v1.z + v0.w * v1.y + v0.y * v1.w;
			result.z = v0.x * v1.y - v0.y * v1.x + v0.w * v1.z + v0.z * v1.w;
			result.w = v0.w * v1.w - (v0.x*v1.x + v0.y*v1.y + v0.z*v1.z);

			v0 = result;
			return result;
		}

		template <typename T>
		inline Quaternion<T> operator*(const Quaternion<T>& v0, const Quaternion<T>& v1)
		{
			Quaternion<T> result;

			result.x = v0.y * v1.z - v0.z * v1.y + v0.w * v1.x + v0.x * v1.w;
			result.y = v0.z * v1.x - v0.x * v1.z + v0.w * v1.y + v0.y * v1.w;
			result.z = v0.x * v1.y - v0.y * v1.x + v0.w * v1.z + v0.z * v1.w;
			result.w = v0.w * v1.w - (v0.x*v1.x + v0.y*v1.y + v0.z*v1.z);

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
			float cosTheta = dot(q0.AsVec4(), q1.AsVec4());
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
				float sine = sqrtf(1.0f - cosTheta*cosTheta);
				float angle = atan2f(sine, cosTheta);
				float invSine = 1.0f / sine;
				float coeff0 = sinf((1.0f - t) * angle) * invSine;
				float coeff1 = sinf(t * angle) * invSine;

				result = q1*coeff0 + q2*coeff1;
			}
			else
			{
				// If the angle is small, use linear interpolation
				result = q0*(1.0f - t) + q2*t;
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
			Quaternion<T> conjugate = Conjugate(q);
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

		typedef struct Quaternion<f32> quat;
		static const quat QUAT_UNIT = quat(0.0f, 0.0f, 0.0f, 1.0f);

		///// MATRIX
		template <typename T, u32 ROWS, u32 COLUMNS>
		struct Matrix
		{
			Matrix<T, ROWS, COLUMNS>()
			{
				memset(data, 0, ROWS*COLUMNS * sizeof(T));
			}

			~Matrix<T, ROWS, COLUMNS>() {}

			T& operator[](u32 index)
			{
				return data[index];
			}

			T data[ROWS*COLUMNS];
		};

		//3x3 Matrix
		template <typename T>
		struct Matrix<T, 3, 3>
		{
			Matrix<T, 3, 3>()
			{
				memset(data, 0, 9 * sizeof(T));
			}

			~Matrix<T, 3, 3>() {}

			T& operator[](u32 index)
			{
				return data[index];
			}

			void setIdentity()
			{
				memset(data, 0, 9 * sizeof(T));
				c00 = c11 = c22 = 1.0f;
			}

			void setScale(const T sx, const T sy, const T sz)
			{
				memset(data, 0, 9 * sizeof(T));
				c00 = sx;
				c11 = sy;
				c22 = sz;
			}

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

		//4x4 Matrix
		template <typename T>
		struct Matrix<T, 4, 4>
		{
			Matrix<T, 4, 4>()
			{
				memset(data, 0, 16 * sizeof(T));
			}

			Matrix<T, 4, 4>(const Matrix<T, 4, 4>& m)
			{
				memcpy(data, m.data, 16 * sizeof(T));
			}

			Matrix<T, 4, 4>(const T* coefficients)
			{
				memcpy(data, coefficients, 16 * sizeof(T));
			}

			~Matrix<T, 4, 4>() {}

			void operator=(const Matrix<T, 4, 4>& m)
			{
				memcpy(data, m.data, 16 * sizeof(T));
			}

			void operator=(const T* coefficients)
			{
				memcpy(data, coefficients, 16 * sizeof(T));
			}

			T& operator[](u32 index)
			{
				return data[index];
			}

			const T& operator[](u32 index) const
			{
				return data[index];
			}

			T& operator()(u8 x, u8 y)
			{
				return data[x * 4 + y];
			}

			const T& operator()(u8 x, u8 y) const
			{
				return data[x * 4 + y];
			}

			void setIdentity()
			{
				memset(data, 0, 16 * sizeof(T));
				c00 = c11 = c22 = c33 = 1.0f;
			}

			void setScale(const T sx, const T sy, const T sz)
			{
				memset(data, 0, 9 * sizeof(T));
				c00 = sx;
				c11 = sy;
				c22 = sz;
			}

			void setTranslation(const vec3& translation)
			{
				data[12] = translation.x;
				data[13] = translation.y;
				data[14] = translation.z;
			}

			void transpose()
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

		//Matrix multiplication
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
		inline Matrix<T, 4, 4> computeTransform(const Vector<T, 3>& translation, const Vector<T, 3>& scale, const Quaternion<T>& rotation)
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
		inline Matrix<T, 4, 4> computeInverseTransform(const Matrix<T, 4, 4>& m)
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

			result[12] = -((m[0] * m[12]) + (m[1] * m[13]) + (m[2] * m[14]) + (m[3] * m[15]));
			result[13] = -((m[4] * m[12]) + (m[5] * m[13]) + (m[6] * m[14]) + (m[7] * m[15]));
			result[14] = -((m[8] * m[12]) + (m[9] * m[13]) + (m[10] * m[14]) + (m[11] * m[15]));
			result[15] = 1.0f;


			return result;
		}

		//Inverse of a matrix
		template <typename T>
		inline bool computeInverse(const Matrix<T, 4, 4>& m, Matrix<T, 4, 4>& result)
		{

			result[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
			result[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
			result[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
			result[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
			result[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
			result[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
			result[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
			result[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
			result[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
			result[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
			result[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
			result[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
			result[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
			result[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
			result[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
			result[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

			f32 determinant = m[0] * result[0] + m[1] * result[4] + m[2] * result[8] + m[3] * result[12];
			if (determinant != 0.0f)
			{
				determinant = 1.0f / determinant;

				for (int i = 0; i < 16; i++)
				{
					result[i] *= determinant;
				}
				return true;
			}

			return false;
		}

		template <typename T>
		inline Matrix<T, 4u, 4u> computePerspectiveProjectionMatrix(T fov, T aspect, T n, T f)
		{
			Matrix<T, 4u, 4u> result = {};
			T height = tan(fov * T(0.5))*n;
			T width = height * aspect;

			result[0] = n / width;
			result[5] = -n / height;
			result[10] = -(f + n) / (f - n);
			result[11] = -1.0f;
			result[14] = (-2.0f*f*n) / (f - n);

			return result;
		}


		template <typename T>
		inline Matrix<T, 4, 4> computeOrthographicProjectionMatrix(T left, T right, T bottom, T top, T near, T far)
		{
			Matrix<T, 4, 4> result;

			T deltaX = right - left;
			T deltaY = top - bottom;
			T deltaZ = far - near;


			result[0] = 2.0f / deltaX;
			result[1] = 0.0f;
			result[2] = 0.0f;
			result[3] = 0.0f;

			result[4] = 0.0f;
			result[5] = 2.0f / deltaY;
			result[6] = 0.0f;
			result[7] = 0.0f;

			result[8] = 0.0f;
			result[9] = 0.0f;
			result[10] = -2.0f / deltaZ;
			result[11] = 0.0f;

			result[12] = -(right + left) / deltaX;
			result[13] = -(top + bottom) / deltaY;
			result[14] = -(far + near) / deltaZ;
			result[15] = 1.0f;

			return result;
		}

		template< typename T>
		inline Vector<T, 4> operator*(const Vector<T, 4>& v, const Matrix<T, 4, 4>& m)
		{
			Vector<T, 4> result;
			result.x = Dot(v, vec4(m.c00, m.c01, m.c02, m.c03));
			result.y = Dot(v, vec4(m.c10, m.c11, m.c12, m.c13));
			result.z = Dot(v, vec4(m.c20, m.c21, m.c22, m.c23));
			result.w = Dot(v, vec4(m.c30, m.c31, m.c32, m.c33));

			return result;
		}

		template< typename T>
		inline Vector<T, 3> operator*(const Vector<T, 3>& v, const Matrix<T, 3, 3>& m)
		{
			Vector<T, 3> result;
			result.x = Dot(v, vec3(m.c00, m.c01, m.c02));
			result.y = Dot(v, vec3(m.c10, m.c11, m.c12));
			result.z = Dot(v, vec3(m.c20, m.c21, m.c22));

			return result;
		}

		//Print
		template <typename T, u32 ROWS, u32 COLUMNS>
		inline std::ostream& operator<<(std::ostream& o, const Matrix<T, ROWS, COLUMNS>& m)
		{
			o << "[";
			for (u32 i(0); i < ROWS; ++i)
			{
				o << "[";
				for (u32 j(0); j < COLUMNS; ++j)
				{
					o << m.data[i*COLUMNS + j];
					if (j != COLUMNS - 1)
						o << ",";
				}
				o << "]";
			}
			o << "]";

			return o;
		}

		typedef Matrix<f32, 3u, 3u> mat3;
		typedef Matrix<f32, 4u, 4u> mat4;

	} //math namespace
}//bkk namespace
#endif  /*  MATH_H */
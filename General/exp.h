#pragma once

namespace Math
{
	template<typename T>
	class CExp
	{
		template<typename T>
		friend inline CExp<T> &operator +=(CExp<T> &left, T right);
		template<typename T>
		friend inline CExp<T> &operator -=(CExp<T> &left, T right);
		template<typename T>
		friend inline CExp<T> &operator +=(CExp<T> &left, CExp<T> right);
		template<typename T>
		friend inline CExp<T> &operator -=(CExp<T> &left, CExp<T> right);
	public:
		CExp(T value = T()) : _value(value) {}
		operator T() const { return _value; }
		operator T &() { return _value; }
	private:
		T _value;
	};

	/*
	+=/-=(CExp<T> &, CExp<T>) performed via +=/-=(CExp<T> &, T) with ctor CExp(T) for T
	operator T &() can not be used to substitude +=/-=(CExp<T> &, T) with +=/-=(T, CExp<T>)
	because +=/-=(CExp<T> &, T) will use default +=/-=(T &, T)
	*/

	template<typename T>
	inline CExp<T> &operator +=(CExp<T> &left, T right)
	{
		left._value += right * left;
		return left;
	}

	template<typename T>
	inline CExp<T> &operator -=(CExp<T> &left, T right)
	{
		left._value -= right * left;
		return left;
	}

	template<typename T>
	inline T &operator +=(T &left, CExp<T> right)
	{
		return left += right * left;
	}

	template<typename T>
	inline T &operator -=(T &left, CExp<T> right)
	{
		return left -= right * left;
	}

	template<typename T>
	inline CExp<T> &operator +=(CExp<T> &left, CExp<T> right)
	{
		left._value += right * left;
		return left;
	}

	template<typename T>
	inline CExp<T> &operator -=(CExp<T> &left, CExp<T> right)
	{
		left._value -= right * left;
		return left;
	}

	/*
	+/- (CExp<T>, T), (T, CExp<T>):
	T param converted to CExp<T> instead of vice versa due to ctor have higher priority than cast operator
	*/

	template<typename T>
	inline CExp<T> operator +(CExp<T> left, CExp<T> right)
	{
		return left += right;
	}

	template<typename T>
	inline CExp<T> operator -(CExp<T> left, CExp<T> right)
	{
		return left -= right;
	}
}
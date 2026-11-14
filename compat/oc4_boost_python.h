#pragma once

// The subset of Boost.Python 1.32 used by Civ4's CvGameCoreDLL, implemented on
// pybind11 so the SDK's Cy*Interface*.cpp binding files (and mods' copies of
// them) compile unchanged against Python 2.7.
//
// Semantics kept from Boost.Python:
// * class_<T> exposes T's default constructor when T has one.
// * return_value_policy<reference_existing_object> / <manage_new_object>.
// * enum_<E> creates an int subclass whose values print as their names; C++
//   functions taking enums accept plain Python ints, and returned enums are
//   instances of the registered type (plain ints when the enum is unregistered).
// * Everything is published into the module "CvPythonExtensions".

#include <cmath> // before Python.h: Python 2.7's pyconfig.h #defines hypot

#include <pybind11/cast.h>

namespace pybind11::detail
{
	// std::string and char strings reach Python 2 as str (bytes), as with Boost.Python, not
	// as pybind11's unicode: scripts pickle them (CyGame.getScriptData) and compare them with
	// str literals. (Declared before anything instantiates the caster; char* casts go
	// through it.) Wide strings stay unicode.
	template <>
	struct type_caster<std::string> : string_caster<std::string>
	{
		using string_caster<std::string>::cast;
		static handle cast(const std::string& src, return_value_policy, handle)
		{
			return PyString_FromStringAndSize(src.data(), static_cast<Py_ssize_t>(src.size()));
		}
	};
}

#include <pybind11/pybind11.h>

#include <string>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>

namespace oc4::pycompat
{
	// The module the game DLL publishes into.
	pybind11::module_ currentModule();

	// Enum types are registered in a dict on the module, keyed by typeid name, so
	// every binary (game DLL, host) sees the same Python types.
	pybind11::object makeEnumType(const char* name);
	void registerEnumType(const char* typeIdName, pybind11::handle type);
	PyObject* findEnumType(const char* typeIdName); // borrowed, may be null

	template <class E>
	PyObject* enumTypeFor()
	{
		static PyObject* cached = nullptr; // per binary
		if (!cached)
			cached = findEnumType(typeid(E).name());
		return cached;
	}

	PyObject* enumValue(PyObject* type, long value); // new reference
}

namespace pybind11::detail
{
	// All enums cross the boundary as ints (see above).
	template <typename E>
	class type_caster<E, std::enable_if_t<std::is_enum_v<E>>>
	{
	public:
		PYBIND11_TYPE_CASTER(E, const_name("int"));

		bool load(handle src, bool)
		{
			PyObject* o = src.ptr();
			if (!o)
				return false;
			if (PyInt_Check(o))
			{
				value = static_cast<E>(PyInt_AsLong(o));
				return true;
			}
			if (PyLong_Check(o))
			{
				const long v = PyLong_AsLong(o);
				if (v == -1 && PyErr_Occurred())
				{
					PyErr_Clear();
					return false;
				}
				value = static_cast<E>(v);
				return true;
			}
			return false;
		}

		static handle cast(E v, return_value_policy, handle)
		{
			if (PyObject* type = oc4::pycompat::enumTypeFor<E>())
				return oc4::pycompat::enumValue(type, static_cast<long>(v));
			return PyInt_FromLong(static_cast<long>(v));
		}
	};
}

namespace boost::python
{
	namespace py = pybind11;

	using object = py::object;
	using tuple = py::tuple;
	using list = py::list;
	using dict = py::dict;
	using str = py::str;

	// Boost.Python converts a raw pointer argument by copying the pointee; the SDK relies
	// on that (CyPlayer::firstCity builds a tuple from a new CyCity and deletes it).
	template <class... Args>
	tuple make_tuple(Args&&... args)
	{
		return py::make_tuple<py::return_value_policy::copy>(std::forward<Args>(args)...);
	}

	struct reference_existing_object
	{
	};
	struct manage_new_object
	{
	};
	struct copy_const_reference
	{
	};
	struct copy_non_const_reference
	{
	};
	struct return_by_value
	{
	};

	template <class Policy>
	struct return_value_policy
	{
	};

	template <class... B>
	struct bases
	{
	};

	template <class... A>
	struct init
	{
	};

	struct no_init_t
	{
	};
	inline constexpr no_init_t no_init{};

	// Keyword names for BOOST_PYTHON_*_OVERLOADS; Civ4 scripts call positionally,
	// so names are kept for documentation only.
	struct args_
	{
		template <class... S>
		explicit args_(S... names) : count(sizeof...(S))
		{
			((void)names, ...);
		}
		size_t count = 0;
	};

	template <class... S>
	args_ args(S... names)
	{
		return args_(names...);
	}

	struct overloads_base
	{
		const char* doc = "";
	};

	// min == max binds as a plain function (all the SDK uses). With min < max (mods: member
	// functions with default arguments), class_::def registers one overload per argument
	// count, each calling through the qualified name so that C++ fills in the defaults.
#define BOOST_PYTHON_MEMBER_FUNCTION_OVERLOADS(name, fn, minArgs, maxArgs)                          \
	static_assert((minArgs) <= (maxArgs), "openciv4 compat: minArgs > maxArgs");                 \
	struct name : boost::python::overloads_base                                                   \
	{                                                                                             \
		static constexpr std::size_t min_args = (minArgs);                                        \
		static constexpr std::size_t max_args = (maxArgs);                                        \
		template <class S, class... A>                                                            \
		static decltype(auto) invoke(S& self, A... a) { return self.fn(a...); }                   \
		name(boost::python::args_, const char* d = "") { doc = d; }                               \
	};
#define BOOST_PYTHON_FUNCTION_OVERLOADS(name, fn, minArgs, maxArgs)                                 \
	static_assert((minArgs) == (maxArgs), "openciv4 compat: defaulted free-function overloads not supported"); \
	struct name : boost::python::overloads_base                                                   \
	{                                                                                             \
		name(boost::python::args_, const char* d = "") { doc = d; }                               \
	};

	namespace compat_detail
	{
		template <class P>
		constexpr py::return_value_policy policyOf()
		{
			if constexpr (std::is_same_v<P, reference_existing_object>)
				return py::return_value_policy::reference;
			else if constexpr (std::is_same_v<P, manage_new_object>)
				return py::return_value_policy::take_ownership;
			else if constexpr (std::is_same_v<P, return_by_value>)
				return py::return_value_policy::move;
			else
				return py::return_value_policy::copy;
		}

		template <class T>
		struct isPolicy : std::false_type
		{
		};
		template <class P>
		struct isPolicy<return_value_policy<P>> : std::true_type
		{
			static constexpr py::return_value_policy policy = policyOf<P>();
		};

		// Defaulted member overloads: the first N parameter types of a member function.
		template <class F>
		struct memberFn;
		template <class R, class C, class... P>
		struct memberFn<R (C::*)(P...)>
		{
			using cls = C;
			using ret = R;
			using params = std::tuple<P...>;
		};
		template <class R, class C, class... P>
		struct memberFn<R (C::*)(P...) const>
		{
			using cls = const C;
			using ret = R;
			using params = std::tuple<P...>;
		};
		template <class Tuple, std::size_t... I>
		auto takeImpl(std::index_sequence<I...>) -> std::tuple<std::tuple_element_t<I, Tuple>...>;
		template <std::size_t N, class Tuple>
		using take_t = decltype(takeImpl<Tuple>(std::make_index_sequence<N>{}));
		template <class O, class C, class R, class Tuple>
		struct overloadCall;
		template <class O, class C, class R, class... P>
		struct overloadCall<O, C, R, std::tuple<P...>>
		{
			static R call(C& self, P... p) { return O::invoke(self, p...); }
		};
		template <class O>
		concept defaultedOverloads = std::is_base_of_v<overloads_base, O> && requires { O::max_args; } && (O::min_args < O::max_args);

		// Boost call extras -> pybind11 extras (docstrings pass through).
		template <class E>
		decltype(auto) translate(E&& e)
		{
			if constexpr (isPolicy<std::decay_t<E>>::value)
				return isPolicy<std::decay_t<E>>::policy;
			else if constexpr (std::is_base_of_v<overloads_base, std::decay_t<E>>)
				return e.doc;
			else
				return std::forward<E>(e);
		}

		// py::class_<T, Kept...> where Kept are the listed bases T really derives
		// from. Boost.Python tolerated bogus entries (the SDK lists CvScalableInfo
		// for CvUnitInfo/CvBuildingInfo, which do not inherit it); pybind11 does not.
		template <class... B>
		struct typeList
		{
		};

		template <class T, class Kept, class... B>
		struct filterBases;
		template <class T, class... Kept>
		struct filterBases<T, typeList<Kept...>>
		{
			using type = py::class_<T, Kept...>;
		};
		template <class T, class... Kept, class B, class... Rest>
		struct filterBases<T, typeList<Kept...>, B, Rest...>
		{
			using type = std::conditional_t<std::is_base_of_v<B, T>, typename filterBases<T, typeList<Kept..., B>, Rest...>::type,
				typename filterBases<T, typeList<Kept...>, Rest...>::type>;
		};

		template <class T, class X>
		struct pyClassFor
		{
			using type = py::class_<T>;
		};
		template <class T, class... B>
		struct pyClassFor<T, bases<B...>>
		{
			using type = typename filterBases<T, typeList<>, B...>::type;
		};
	}

	// The current scope is the module being populated.
	inline py::module_ scope() { return oc4::pycompat::currentModule(); }

	template <class T, class X1 = void, class X2 = void, class X3 = void>
	class class_
	{
		using PyClass = typename compat_detail::pyClassFor<T, X1>::type;

	public:
		explicit class_(const char* name) : m_cls(oc4::pycompat::currentModule(), name)
		{
			if constexpr (std::is_default_constructible_v<T>)
				m_cls.def(py::init<>());
		}

		class_(const char* name, const char* doc) : m_cls(oc4::pycompat::currentModule(), name, doc)
		{
			if constexpr (std::is_default_constructible_v<T>)
				m_cls.def(py::init<>());
		}

		class_(const char* name, no_init_t) : m_cls(oc4::pycompat::currentModule(), name) {}

		template <class... A>
		class_(const char* name, init<A...>) : m_cls(oc4::pycompat::currentModule(), name)
		{
			m_cls.def(py::init<A...>());
		}

		template <class F, class... E>
		class_& def(const char* name, F&& f, E&&... extras)
		{
			m_cls.def(name, std::forward<F>(f), compat_detail::translate(std::forward<E>(extras))...);
			return *this;
		}

		// A member function with default arguments: one pybind11 overload per argument count.
		template <class F, class O, class... E>
			requires compat_detail::defaultedOverloads<std::decay_t<O>>
		class_& def(const char* name, F, O&& overloads, E&&... extras)
		{
			using Ovl = std::decay_t<O>;
			defDefaulted<Ovl, F>(name, overloads.doc, std::make_index_sequence<Ovl::max_args - Ovl::min_args + 1>{}, extras...);
			return *this;
		}

		template <class... A, class... E>
		class_& def(init<A...>, E&&... extras)
		{
			m_cls.def(py::init<A...>(), std::forward<E>(extras)...);
			return *this;
		}

		template <class D, class C>
		class_& def_readwrite(const char* name, D C::*member)
		{
			m_cls.def_readwrite(name, member);
			return *this;
		}

		template <class D, class C>
		class_& def_readonly(const char* name, const D C::*member)
		{
			m_cls.def_readonly(name, member);
			return *this;
		}

	private:
		template <class Ovl, class F, std::size_t... K, class... E>
		void defDefaulted(const char* name, const char* doc, std::index_sequence<K...>, const E&... extras)
		{
			using M = compat_detail::memberFn<F>;
			(m_cls.def(name,
				&compat_detail::overloadCall<Ovl, typename M::cls, typename M::ret,
					compat_detail::take_t<Ovl::min_args + K, typename M::params>>::call,
				doc, compat_detail::translate(extras)...), ...);
		}

		PyClass m_cls;
	};

	template <class F, class... E>
	void def(const char* name, F&& f, E&&... extras)
	{
		oc4::pycompat::currentModule().def(name, std::forward<F>(f), compat_detail::translate(std::forward<E>(extras))...);
	}

	template <class E>
	class enum_
	{
	public:
		explicit enum_(const char* name) : m_type(oc4::pycompat::makeEnumType(name))
		{
			oc4::pycompat::currentModule().attr(name) = m_type;
			oc4::pycompat::registerEnumType(typeid(E).name(), m_type);
		}

		enum_& value(const char* name, E v)
		{
			const long n = static_cast<long>(v);
			py::object inst = m_type(n);
			m_type.attr(name) = inst;
			m_type.attr("names")[py::reinterpret_steal<py::object>(PyString_FromString(name))] = inst;
			m_type.attr("values")[py::int_(n)] = inst;
			m_type.attr("_oc4names")[py::int_(n)] = py::reinterpret_steal<py::object>(PyString_FromString(name));
			return *this;
		}

		enum_& export_values()
		{
			py::module_ m = oc4::pycompat::currentModule();
			for (auto item : py::reinterpret_borrow<py::dict>(m_type.attr("names")))
				m.attr(item.first) = item.second;
			return *this;
		}

	private:
		py::object m_type;
	};
}

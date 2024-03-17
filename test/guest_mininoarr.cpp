#include <doarr/export.hpp>

namespace noarr {
	template<class T>
	concept Struct = T::is_struct;
	template<class T>
	concept Proto = T::is_proto;

	template<class T>
	struct scalar {
		static constexpr bool is_struct = true;
		static constexpr bool is_proto = false;
	};

	template<char Dim, Struct S>
	struct vector_t {
		static constexpr bool is_struct = true;
		static constexpr bool is_proto = false;
		S s;
	};

	template<char Dim>
	struct vector {
		static constexpr bool is_struct = false;
		static constexpr bool is_proto = true;
		constexpr auto instantiate_and_construct(Struct auto s) {
			return vector_t<Dim, decltype(s)>{s};
		}
	};

	template<char Dim, Struct S>
	struct sized_vector_t {
		static constexpr bool is_struct = true;
		static constexpr bool is_proto = false;
		S s;
		std::size_t size;
	};

	template<char Dim>
	struct sized_vector {
		static constexpr bool is_struct = false;
		static constexpr bool is_proto = true;
		std::size_t size;
		constexpr auto instantiate_and_construct(Struct auto s) {
			return sized_vector_t<Dim, decltype(s)>{s, size};
		}
	};

	////////////////////////////////////////////////////////////////

	constexpr Struct auto operator ^(Struct auto t, Proto auto u) {
		return u.instantiate_and_construct(t);
	}

	template<Proto T, Proto U>
	struct proto_compose {
		static constexpr bool is_struct = false;
		static constexpr bool is_proto = true;
		T t;
		U u;
		constexpr auto instantiate_and_construct(Struct auto s) {
			return s ^ t ^ u;
		}
	};

	constexpr Proto auto operator ^(Proto auto t, Proto auto u) {
		return proto_compose{t, u};
	}

	////////////////////////////////////////////////////////////////

	template<class T>
	struct bag {
		static_assert(T::is_struct);
		T t;
		void *ptr;
	};

	constexpr auto make_bag(auto t, void *ptr) {
		return bag<decltype(t)>{t, ptr};
	}

	////////////////////////////////////////////////////////////////

	template<std::size_t I>
	struct lit_t {
	};

	template<std::size_t I>
	constexpr lit_t<I> lit;
}

doarr::exported nempty(auto t) {
	(void) t;
}

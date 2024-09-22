/*************************************************************************/
/* delegate.hpp                                                          */
/*************************************************************************/
/* https://www.dandevlog.com/all/programming/351/                        */
/* https://github.com/dandevlog0206/cpp-delegate                         */
/*************************************************************************/
/* Copyright (c) 2024 www.dandevlog.com                                  */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#pragma once

#include <type_traits>
#include <utility>
#include <cstring> // memcmp
#include <assert.h>

#if defined(__GNUC__) || defined(__GNUG__)
#define DEL_USES_GCC
#define DEL_INLINE inline
#elif defined(_MSC_VER) 
#define DEL_USES_MSVC
#define DEL_INLINE __forceinline
#endif

#if defined(DEL_USES_GCC)
template <class Virtual_ptr>
ptrdiff_t _virtual_indexof(Virtual_ptr vfptr) {
	typedef struct {
		void** functionPtr;
		size_t offset;
	} MP;

	MP& t = *(MP*)&vfptr;
	if ((intptr_t)t.functionPtr & 1) {
		return (t.functionPtr - (void**)1);
	} else return -1;
}
#elif defined(DEL_USES_MSVC)
template <class Virtual_ptr>
ptrdiff_t _virtual_indexof(Virtual_ptr vfptr) {
	union {
		Virtual_ptr vptr;
		uint32_t* uint32_ptr;
		uint8_t* uint8_ptr;
	} u;

	u.vptr = vfptr;
	if (*u.uint8_ptr == 0xE9) { // for incremental link
		u.uint8_ptr++;
		u.uint8_ptr += *u.uint32_ptr + 4;
	}

	if constexpr (sizeof(nullptr) == 8) { // x64
		if (*(u.uint32_ptr++) != 0xFF018B48) return -1;
	} else { // x86
		if ((*(u.uint32_ptr) & 0x00ffffff) != 0xFF018B) return -1;
		u.uint8_ptr += 3;
	}

	switch (*(u.uint8_ptr++)) {
	case 0x20: return 0;
	case 0x60: return *u.uint8_ptr / sizeof(nullptr);
	case 0xA0: return *u.uint32_ptr / sizeof(nullptr);
	}
	return -1;
}
#else 
template <class Virtual_ptr>
ptrdiff_t _virtual_indexof(Virtual_ptr vfptr) {
	static_assert("you have to implement your own");
}
#endif

#ifdef DEL_USES_MSVC
#ifndef __VECTOR_C
class __single_inheritance _generic_class_t;
#endif
class _generic_class_t {};
#else
class _generic_class_t;
#endif

template <int N>
struct _Simplify_func {
	template <class Class, class Method, class GenericMemFuncType>
	inline static _generic_class_t* _Convert(Class* pthis, Method function_to_bind, GenericMemFuncType& bound_func) {
		static_assert("Unsupported member function pointer on_this compiler");
		return nullptr;
	}
};

constexpr size_t _SINGLE_INHERITANCE_PTR_SIZE = sizeof(void (_generic_class_t::*)());

template <>
struct _Simplify_func<_SINGLE_INHERITANCE_PTR_SIZE> {
	template <class Class, class Method, class GenericMemFuncType>
	inline static _generic_class_t* _Convert(Class* pthis, Method function_to_bind, GenericMemFuncType& bound_func) {
		bound_func = reinterpret_cast<GenericMemFuncType>(function_to_bind);
		return reinterpret_cast<_generic_class_t*>(pthis);
	}
};

#ifdef DEL_USES_MSVC
struct _generic_virtual_class_t : virtual public _generic_class_t {
	_generic_virtual_class_t* _This() { return this; }
	using _ProbePtrType = decltype(&_This);
};

constexpr size_t _MULTIPLE_INHERITANCE_PTR_SIZE = _SINGLE_INHERITANCE_PTR_SIZE + sizeof(int) * 1;
constexpr size_t _VIRTUAL_INHERITANCE_PTR_SIZE  = _SINGLE_INHERITANCE_PTR_SIZE + sizeof(int) * 2;
constexpr size_t _UNKNOWN_INHERITANCE_PTR_SIZE  = _SINGLE_INHERITANCE_PTR_SIZE + sizeof(int) * 3;

template<>
struct _Simplify_func<_MULTIPLE_INHERITANCE_PTR_SIZE> {
	template <class Class, class Method, class GenericMemFuncType>
	inline static _generic_class_t* _Convert(Class* pthis, Method function_to_bind, GenericMemFuncType& bound_func) {
		// In MSVC, a multiple inheritance member pointer is internally defined as:
		union {
			Method func;
			struct {
				GenericMemFuncType funcaddress;
				int delta;
			}s;
		} u;

		static_assert(sizeof(function_to_bind) == sizeof(u.s));
		u.func = function_to_bind;
		bound_func = u.s.funcaddress;
		return reinterpret_cast<_generic_class_t*>(reinterpret_cast<char*>(pthis) + u.s.delta);
	}
};

template <>
struct _Simplify_func<_VIRTUAL_INHERITANCE_PTR_SIZE> {
	template <class Class, class Method, class GenericMemFuncType>
	inline static _generic_class_t* _Convert(Class* pthis, Method function_to_bind, GenericMemFuncType& bound_func) {
		struct MicrosoftVirtualMFP {
			void (_generic_class_t::* codeptr)(); // points to the actual member function
			int delta;                           // #bytes to be added to the 'this' pointer
			int vtable_index;                    // or 0 if no virtual inheritance
		};

		union {
			Method func;
			_generic_class_t* (Class::* ProbeFunc)();
			MicrosoftVirtualMFP s;
		} u;

		union {
			_generic_virtual_class_t::_ProbePtrType virtfunc;
			MicrosoftVirtualMFP s;
		} u2;

		static_assert(sizeof(function_to_bind) == sizeof(u.s));
		static_assert(sizeof(function_to_bind) == sizeof(u.ProbeFunc));
		static_assert(sizeof(u2.virtfunc) == sizeof(u2.s));

		u.func = function_to_bind;
		bound_func = reinterpret_cast<GenericMemFuncType>(u.s.codeptr);
		u2.virtfunc = &_generic_virtual_class_t::_This;
		u.s.codeptr = u2.s.codeptr;
		return (pthis->*u.ProbeFunc)();
	}
};

template <>
struct _Simplify_func<_UNKNOWN_INHERITANCE_PTR_SIZE> {
	template <class Class, class Method, class GenericMemFuncType>
	inline static _generic_class_t* _Convert(Class* pthis, Method function_to_bind, GenericMemFuncType& bound_func) {
		union {
			Method func;
			struct {
				GenericMemFuncType m_funcaddress;
				int delta;
				int vtordisp;
				int vtable_index;
			} s;
		} u;

		static_assert(sizeof(Method) == sizeof(u.s));
		u.func = function_to_bind;
		bound_func = u.s.funcaddress;
		int virtual_delta = 0;
		if (u.s.vtable_index) {
			const int* vtable = *reinterpret_cast<const int* const*>(
				reinterpret_cast<const char*>(pthis) + u.s.vtordisp);

			virtual_delta = u.s.vtordisp + *reinterpret_cast<const int*>(
				reinterpret_cast<const char*>(vtable) + u.s.vtable_index);
		}

		return reinterpret_cast<_generic_class_t*>(reinterpret_cast<char*>(pthis) + u.s.delta + virtual_delta);
	};
};
#endif // MS/Intel hacks

template<class Ret, class...Args>
struct _Stateful_wrapper abstract {
	virtual Ret call(Args... args)   = 0;
	virtual _generic_class_t* copy() = 0;
};

template<class T>
struct _Closure_ptr;

template<class Ret, class...Args>
struct _Closure_ptr<Ret(Args...)> {
	using _generic_class_ptr_t   = _generic_class_t*;
	using _generic_memfunc_ptr_t = void (_generic_class_t::*)();
	using _fptr_t                = Ret(_generic_class_t::*)(Args...);

	DEL_INLINE _generic_class_ptr_t get_inst() const {
		return inst;
	}

	DEL_INLINE _fptr_t get_fptr() const {
		return reinterpret_cast<_fptr_t>(fptr);
	}

	template <class Obj, class Method>
	DEL_INLINE void bind_method(Obj* ptr, Method to_bind) {
		clear();
		inst = _Simplify_func<sizeof(Method)>::_Convert(ptr, to_bind, fptr);
		
		if constexpr (std::is_polymorphic_v<Obj>) {
			auto idx = _virtual_indexof(fptr);
			if (idx == -1) return;
			
			auto vtable = *(_generic_memfunc_ptr_t**)inst;
			fptr = vtable[idx];
		}
	}

	template <class Obj, class Method>
	DEL_INLINE void bind_method(const Obj* ptr, Method to_bind) {
		bind_method(const_cast<Obj*>(ptr), to_bind);
	}

	template <class Static_func>
	DEL_INLINE void bind_static(Static_func to_bind) {
		clear();
		if (to_bind == nullptr) fptr = nullptr;
		else bind_method(this, &_Closure_ptr::static_stub);

		// WARNING
		inst = reinterpret_cast<_generic_class_t*>(to_bind);
	}

	template <class Stateful>
	DEL_INLINE void bind_stateful(Stateful obj) {
		clear();
		if constexpr (sizeof(Stateful) > sizeof(nullptr))
			impl_bind_dynamic_stateful(std::forward<Stateful>(obj));
		else impl_bind_static_stateful(std::forward<Stateful>(obj));
	}

	DEL_INLINE void copy_from(const _Closure_ptr& rhs) {
		if (is_dynamic_stateful())
			inst = reinterpret_cast<_Stateful_wrapper<Ret, Args...>*>(rhs.inst)->copy();
		else inst = rhs.inst;

		fptr = rhs.fptr;
	}

	DEL_INLINE void move_from(_Closure_ptr&& rhs) {
		inst = std::exchange(rhs.inst, nullptr);
		fptr = std::exchange(rhs.fptr, nullptr);
	}

	DEL_INLINE void clear() {
		if (is_dynamic_stateful())
			delete inst;
		inst = nullptr;
		fptr = nullptr;
	}

	DEL_INLINE bool empty() const {
		return !(inst || fptr);
	}

	DEL_INLINE bool is_static() const {
		return fptr == reinterpret_cast<_generic_memfunc_ptr_t>(&_Closure_ptr::static_stub);
	}

	DEL_INLINE bool is_dynamic_stateful() const {
		return fptr == reinterpret_cast<_generic_memfunc_ptr_t>(&_Stateful_wrapper<Ret, Args...>::call);
	}

	DEL_INLINE bool is_equal_static(Ret(*rhs)(Args...)) const {
		return inst == rhs;
	}

	DEL_INLINE bool is_equal(const _Closure_ptr& rhs) const {
		auto stateful0 = is_dynamic_stateful();
		auto stateful1 = rhs.is_dynamic_stateful();
		if (stateful0 && stateful1)
			return (*(_generic_memfunc_ptr_t**)inst)[0] == (*(_generic_memfunc_ptr_t**)rhs.inst)[0];
		else if (!stateful0 && !stateful1) 
			return inst == rhs.inst && fptr == rhs.fptr;
		return false;
	}

private:
	template <class Stateful>
	DEL_INLINE void impl_bind_dynamic_stateful(Stateful&& obj) {
		struct _Wrapper : public _Stateful_wrapper<Ret, Args...> {
			_Wrapper(Stateful&& obj) : obj(std::forward<Stateful>(obj)) {}

			Ret call(Args... args) override {
				return obj.operator()(std::forward<Args>(args)...);
			}

			_generic_class_t* copy() override {
				return reinterpret_cast<_generic_class_t*>(new _Wrapper(*this));
			}

			Stateful obj;
		};

		inst = reinterpret_cast<_generic_class_ptr_t>(new _Wrapper(std::forward<Stateful>(obj)));
		fptr = reinterpret_cast<_generic_memfunc_ptr_t>(&_Stateful_wrapper<Ret, Args...>::call);
	}

	template <class Stateful>
	DEL_INLINE void impl_bind_static_stateful(Stateful&& obj) {
		struct _Wrapper {
			_Wrapper(Stateful&& obj) : obj(std::forward<Stateful>(obj)) {}

			Ret call(Args... args) {
				auto temp = this;
				return (*(_Wrapper*)&temp).obj.operator()(std::forward<Args>(args)...);
			}

			Stateful obj;
		} wrapper(std::forward<Stateful>(obj));

		inst = *(_generic_class_ptr_t*)&wrapper;
		fptr = reinterpret_cast<_generic_memfunc_ptr_t>(&_Wrapper::call);
	}

	// WARNING
	Ret static_stub(Args... args) {
		using _static_func_ptr_t = Ret(*)(Args...);
		return (*reinterpret_cast<_static_func_ptr_t>(this))(std::forward<Args>(args)...);
	}

	_generic_class_ptr_t   inst = nullptr;
	_generic_memfunc_ptr_t fptr = nullptr;
};

template <typename T> class Delegate;

template<class Ret, class...Args>
class Delegate<Ret(Args...)> {
	template<class Ret, class...Args>
	struct _Stateful_wrapper abstract {
		virtual Ret call(Args... args) = 0;
		virtual _generic_class_t* copy() = 0;
	};

public:
	using type     = Delegate;
	using return_t = Ret;

	DEL_INLINE Delegate() {
		closure.clear();
	}

	DEL_INLINE Delegate(Ret(*function_to_bind)(Args...)) {
		closure.bind_static(function_to_bind);
	}

	template <class Class, class Obj>
	DEL_INLINE Delegate(Obj* obj, Ret(Class::* to_bind)(Args...)) {
		closure.bind_method(static_cast<Class*>(obj), to_bind);
	}

	template <class Class, class Obj>
	DEL_INLINE Delegate(const Obj* obj, Ret(Class::* to_bind)(Args...) const) {
		closure.bind_method(static_cast<const Class*>(obj), to_bind);
	}

	template <class Lambda, class _Ret, class...Args, std::enable_if_t<!std::is_same_v<Lambda, Delegate<_Ret(Args...)>>, int> = 0>
	DEL_INLINE Delegate(Lambda&& lambda) {
		bind(std::forward<Lambda>(lambda));
	}

	DEL_INLINE Delegate(const Delegate& rhs) {
		closure.copy_from(rhs.closure);
	}

	DEL_INLINE Delegate(Delegate&& rhs) noexcept {
		closure.move_from(std::move(rhs.closure));
	}

	DEL_INLINE ~Delegate() {
		closure.clear();
	}

	DEL_INLINE Delegate& operator=(const Delegate& rhs) {
		closure.copy_from(rhs.closure);
		return *this;
	}

	DEL_INLINE Delegate& operator=(Delegate&& rhs) noexcept {
		closure.move_from(std::move(rhs.closure));
		return *this;
	}

	DEL_INLINE Delegate& operator=(Ret(*function_to_bind)(Args...)) {
		closure.bind_static(function_to_bind);
		return *this;
	}

	template <class Lambda, class _Ret, class...Args, std::enable_if_t<!std::is_same_v<Lambda, Delegate<_Ret(Args...)>>, int> = 0>
	DEL_INLINE Delegate& operator=(Lambda&& lambda) {
		bind(std::forward<Lambda>(lambda));
		return *this;
	}

	DEL_INLINE void bind(Ret(*function_to_bind)(Args...)) {
		closure.bind_static(this, &Delegate::invokeStatic, function_to_bind);
	}

	template <class Class, class Obj>
	DEL_INLINE void bind(Obj* pthis, Ret(Class::* function_to_bind)(Args...)) {
		closure.bind_method(static_cast<Class*>(pthis), function_to_bind);
	}

	template <class Class, class Obj>
	DEL_INLINE void bind(const Obj* pthis, Ret(Class::* function_to_bind)(Args...) const) {
		closure.bind_method(static_cast<const Class*>(pthis), function_to_bind);
	}

	template <class Lambda, class _Ret, class...Args, std::enable_if_t<!std::is_same_v<Lambda, Delegate<_Ret(Args...)>>, int> = 0>
	DEL_INLINE void bind(Lambda&& lambda) {
		if constexpr (std::is_convertible_v<Lambda, Ret(*)(Args...)>)
			closure.bind_static((Ret(*)(Args...))lambda);
		else closure.bind_stateful(std::forward<Lambda>(lambda));
	}

	DEL_INLINE void clear() {
		closure.clear();
	}

	DEL_INLINE Ret operator()(Args... args) const {
		return (closure.get_inst()->*(closure.get_fptr()))(std::forward<Args>(args)...);
	}

	DEL_INLINE Ret invoke(Args... args) const {
		assert(!closure.empty() && "tried invoking empty delegate");
		return (closure.get_inst()->*(closure.get_fptr()))(std::forward<Args>(args)...);
	}

	DEL_INLINE bool empty() const {
		return closure.empty();
	}

	DEL_INLINE operator bool() const {
		return !empty();
	}

	DEL_INLINE void* instance() const {
		return closure.is_static() ? nullptr : closure.get_inst();
	}

	DEL_INLINE bool operator==(Ret(*funcptr)(Args...)) const {
		return closure.is_equal_static(funcptr);
	}

	DEL_INLINE bool operator!=(Ret(*funcptr)(Args...)) const {
		return !closure.is_equal_static(funcptr);
	}

	DEL_INLINE bool operator==(const Delegate& rhs) const {
		return closure.is_equal(rhs.closure);
	}

	DEL_INLINE bool operator!=(const Delegate& rhs) const {
		return !closure.is_equal(rhs.closure);
	}

private:
	_Closure_ptr<Ret(Args...)> closure;
};

// Helper functions
template <class Ret, class... Args>
DEL_INLINE Delegate<Ret(Args...)> make_delegate(Ret(*func)(Args...)) {
	return { func };
}

template <class Class, class Obj, class Ret, class... Args>
DEL_INLINE Delegate<Ret(Args...)> make_delegate(Obj* instance, Ret(Class::* func)(Args...)) {
	return { instance, func };
}

template <class Class, class Obj, class Ret, class... Args>
DEL_INLINE Delegate<Ret(Args...)> make_delegate(Obj* instance, Ret(Class::* func)(Args...) const) {
	return { instance, func };
}

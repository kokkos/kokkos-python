/*
//@HEADER
// ************************************************************************
//
//                        Kokkos v. 3.0
//       Copyright (2020) National Technology & Engineering
//               Solutions of Sandia, LLC (NTESS).
//
// Under the terms of Contract DE-NA0003525 with NTESS,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY NTESS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NTESS OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Christian R. Trott (crtrott@sandia.gov)
//
// ************************************************************************
//@HEADER
*/

#pragma once

#include "common.hpp"
#include "fwd.hpp"

//--------------------------------------------------------------------------------------//

// default definition
template <typename... T>
struct concat {
  using type = type_list<T...>;
};

// append to type_list
template <typename... T, typename... Tail>
struct concat<type_list<T...>, Tail...> : concat<T..., Tail...> {};

// combine consecutive type_lists
template <typename... T, typename... U, typename... Tail>
struct concat<type_list<T...>, type_list<U...>, Tail...>
    : concat<type_list<T..., U...>, Tail...> {};

template <typename... T>
using concat_t = typename concat<T...>::type;

//--------------------------------------------------------------------------------------//

template <template <typename> class PredicateT, bool ValueT, typename... T>
struct gather {
  using type = concat_t<std::conditional_t<PredicateT<T>::value == ValueT,
                                           concat_t<T>, type_list<>>...>;
};

template <template <typename> class PredicateT, bool ValueT, typename... T>
using gather_t = typename gather<PredicateT, ValueT, T...>::type;

//--------------------------------------------------------------------------------------//

template <typename...>
struct concrete_view_type_list;

template <typename...>
struct dynamic_view_type_list;

template <size_t Idx>
struct index_val;

//--------------------------------------------------------------------------------------//
//  this is used to mark memory spaces as unavailable
//
template <typename Tp>
struct is_available : std::true_type {};

//--------------------------------------------------------------------------------------//
//  this is used to mark template parameters as implicit
//
template <typename Tp>
struct is_implicit : std::false_type {};

template <typename Tp>
struct is_memory_traits : std::false_type {};

template <unsigned T>
struct is_memory_traits<Kokkos::MemoryTraits<T>> : std::true_type {};

//--------------------------------------------------------------------------------------//
//  this is used to convert Kokkos::Device<ExecSpace, MemSpace> to MemSpace
//
template <typename Tp>
struct remove_device {
  using type = Tp;
};

template <typename ExecT, typename MemT>
struct remove_device<Kokkos::Device<ExecT, MemT>> {
  using type = MemT;
};

template <typename Tp>
using remove_device_t = typename remove_device<Tp>::type;

//--------------------------------------------------------------------------------------//
//  this is used to convert Kokkos::Device<ExecSpace, MemSpace> to MemSpace
//
template <typename Tp>
struct is_default_memory_trait : std::false_type {};

template <>
struct is_default_memory_trait<Kokkos::MemoryTraits<0>> : std::true_type {};

//--------------------------------------------------------------------------------------//
//  this is used to get the view type without the device/execution space
//
template <typename, typename...>
struct view_type;

template <template <typename...> class ViewT, typename ValueT,
          typename... Types>
struct view_type<ViewT<ValueT>, type_list<Types...>> {
  using type = ViewT<ValueT, remove_device_t<Types>...>;
};

template <template <typename...> class ViewT, typename ValueT,
          typename... Types>
struct view_type<ViewT<ValueT>, Types...>
    : view_type<ViewT<ValueT>, gather_t<is_implicit, false, Types...>> {};

template <template <typename...> class ViewT, typename ValueT,
          typename... Types>
struct view_type<ViewT<ValueT, Types...>>
    : view_type<ViewT<ValueT>, gather_t<is_implicit, false, Types...>> {};

template <typename... T>
using view_type_t = typename view_type<T...>::type;

//--------------------------------------------------------------------------------------//
//  ensure that the uniform type does not include the default memory trait
//
namespace Impl {
template <typename, typename...>
struct uniform_view_type;

template <template <typename...> class ViewT, typename ValueT,
          typename... Types>
struct uniform_view_type<ViewT<ValueT>, type_list<Types...>> {
  using type = ViewT<ValueT, Types...>;
};

template <template <typename...> class ViewT, typename ValueT,
          typename... Types>
struct uniform_view_type<ViewT<ValueT>, Types...>
    : uniform_view_type<ViewT<ValueT>,
                        gather_t<is_default_memory_trait, false, Types...>> {};

template <template <typename...> class ViewT, typename ValueT,
          typename... Types>
struct uniform_view_type<ViewT<ValueT, Types...>>
    : uniform_view_type<ViewT<ValueT>,
                        gather_t<is_default_memory_trait, false, Types...>> {};

template <typename... T>
using uniform_view_type_t = typename uniform_view_type<T...>::type;
}  // namespace Impl

//--------------------------------------------------------------------------------------//
//  this is used to extract the uniform type of a view
//
template <typename, typename...>
struct uniform_view_type;

template <typename ValueT, typename... Types>
struct uniform_view_type<Kokkos::View<ValueT, Types...>> {
  using type = Impl::uniform_view_type_t<
      typename Kokkos::View<ValueT, Types...>::uniform_type>;
};

template <typename ValueT, typename... Types>
struct uniform_view_type<Kokkos::DynRankView<ValueT, Types...>> {
  using type = Impl::uniform_view_type_t<Kokkos::DynRankView<ValueT, Types...>>;
};

template <typename... T>
using uniform_view_type_t = typename uniform_view_type<T...>::type;

//--------------------------------------------------------------------------------------//
//  this fixes an issue where uniform_type type on 1D views will convert
//  LayoutRight template parameters to LayoutLeft template parameters since they
//  are compatible (left vs. right doesn't matter in 1D). If ENABLE_LAYOUTS is
//  defined this does not cause problems but if it is not defined, calling
//  create_mirror or create_mirror_view will return a template instantiation
//  that has not been generated.
//
template <typename ViewT, typename UniformT>
struct mirror_view_type {
  using type = UniformT;
};

template <typename ViewValueT, typename ViewLayoutT, typename... ViewExtraT,
          typename UniformValueT, typename UniformLayoutT,
          typename... UniformExtraT>
struct mirror_view_type<
    Kokkos::View<ViewValueT *, ViewLayoutT, ViewExtraT...>,
    Kokkos::View<UniformValueT *, UniformLayoutT, UniformExtraT...>> {
  using type = std::conditional_t<
      !std::is_same<ViewLayoutT, UniformLayoutT>::value &&
          !std::is_pointer<UniformValueT>::value,
      view_type_t<Kokkos::View<UniformValueT *, ViewLayoutT, UniformExtraT...>>,
      Kokkos::View<UniformValueT *, UniformLayoutT, UniformExtraT...>>;
};

template <typename ViewT, typename UniformT>
using mirror_view_type_t = typename mirror_view_type<ViewT, UniformT>::type;

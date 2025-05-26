#pragma once

#include "cppdecl/declarations/data.h"
#include "cppdecl/misc/enum_flags.h"
#include "cppdecl/misc/overload.h"
#include "cppdecl/misc/platform.h"

#include <cassert>
#include <iterator>
#include <version>

namespace cppdecl
{
    enum class SimplifyTypeNamesFlags
    {
        // Fixes for compiler/stdlib-specific quirks:
        // Those should do nothing if applied to other platforms.

        // Remove MSVC's `__ptr32` and `__ptr64` cv-qualifiers on pointers.
        bit_msvc_remove_ptr32_ptr64 = 1 << 0,

        // Replace `std::__cxx11` with `std`.
        bit_libstdcxx_remove_cxx11_namespace_in_std = 1 << 1,
        // Replace `std::__1` with `std`.
        bit_libcpp_remove_1_namespace_in_std = 1 << 2,

        bits_remove_std_version_namespace =
            bit_libstdcxx_remove_cxx11_namespace_in_std |
            bit_libcpp_remove_1_namespace_in_std,


        // Rewrite iterators from their internal names.
        // Note that this is best effort. Some containers reuse iterators from each other, and you will not get entirely consistent names across compilers. There's a table in `tests.cpp` that shows the differences.
        // It's recommended to combine this with `bits_common_remove_defargs` and `bits_remove_std_version_namespace`, because we don't go out of our way to reproduce those things in our output.
        bit_libstdcxx_normalize_iterators = 1 << 3,
        bit_libcpp_normalize_iterators = 1 << 4,
        bit_msvcstl_normalize_iterators = 1 << 5,

        bits_normalize_iterators =
            bit_libstdcxx_normalize_iterators |
            bit_libcpp_normalize_iterators |
            bit_msvcstl_normalize_iterators,


        // Fixes for common C++ stuff:

        // Remove elaborated type specifiers, `typename`, etc.
        bit_common_remove_type_prefix = 1 << 6,

        // Remove `signed`, except from `signed char`.
        bit_common_remove_redundant_signed = 1 << 7,

        // Remove allocators from template parameters.
        bit_common_remove_defarg_allocator = 1 << 8,
        // Remove `std::char_traits<T>` from template parameters of `std::basic_string`.
        // This typically requires `bit_common_remove_defarg_allocator` as well, since we can't remove non-last template arguments,
        //   and having the allocator after this one will prevent it from being removed.
        bit_common_remove_defarg_char_traits = 1 << 9,
        // Remove `std::less<T>` and `std::equal_to<T>` from ordered and unordered containers respectively.
        // This typically requires `bit_common_remove_defarg_allocator` as well, since we can't remove non-last template arguments,
        //   and having the allocator after this one will prevent it from being removed.
        bit_common_remove_defarg_comparator = 1 << 10,
        // Remove `std::hash<T>` from unordered containers.
        // This typically requires `bit_common_remove_defarg_allocator` and `bit_common_remove_defarg_comparator` as well, since we can't remove non-last template arguments,
        //   and having the allocator and comparator after this one will prevent it from being removed.
        bit_common_remove_defarg_hash_functor = 1 << 11,

        // Remove various default arguments from templates.
        bits_common_remove_defargs = bit_common_remove_defarg_allocator | bit_common_remove_defarg_char_traits | bit_common_remove_defarg_comparator | bit_common_remove_defarg_hash_functor,

        // Rewrite `std::basic_string<char>` to `std::string` and such.
        // This typically requires `bit_common_remove_defarg_hash_functor`, `bit_common_remove_defarg_allocator`, and `bit_common_remove_defarg_comparator` as well,
        //   since this expects the default template arguments to be already stripped.
        bit_common_rewrite_template_specializations_as_typedefs = 1 << 12,

        // Various mostly compiler-independent bits.
        // Note that `bits_common_remove_defargs` isn't needed when you get the types from `__PRETTY_FUNCTION__` or equivalent on Clang.
        common =
            bit_common_remove_type_prefix |
            bit_common_remove_redundant_signed |
            bits_common_remove_defargs |
            bit_common_rewrite_template_specializations_as_typedefs,


        // Fixes for C stuff:

        // Rewrite `_Bool` as `bool`.
        bit_c_normalize_bool = 1 << 13,

        c =
            bit_c_normalize_bool,


        // Presents for different compilers:
        // Those should do nothing if applied to the names from the wrong compiler.

        // MSVC and Clang in MSVC-compatible mode.
        compiler_msvc_like = bit_msvc_remove_ptr32_ptr64,

        compiler_all = compiler_msvc_like,
        compiler_current = 0
            #ifdef _MSC_VER
            | compiler_msvc_like
            #endif
            ,

        // Presets for different standard libraries:
        stdlib_libstdcxx =
            bit_libstdcxx_remove_cxx11_namespace_in_std |
            bit_libstdcxx_normalize_iterators,
        stdlib_libcpp =
            bit_libcpp_remove_1_namespace_in_std |
            bit_libcpp_normalize_iterators,
        stdlib_msvcstl =
            bit_msvcstl_normalize_iterators,

        stdlib_all = stdlib_libstdcxx | stdlib_libcpp | stdlib_msvcstl,
        stdlib_current = 0
            #ifdef _GLIBCXX_RELEASE
            | stdlib_libstdcxx
            #endif
            #ifdef _LIBCPP_VERSION
            | stdlib_libstdcxx
            #endif
            #ifdef _MSVC_STL_VERSION
            | stdlib_msvcstl
            #endif
            ,


        // High-level flags:

        // Absolutely every rule we know. Good if the names come from outside of the program.
        all = common | c | compiler_all | stdlib_all,
        // Only the rules that are relevant on the current compiler and standard library.
        native =
            (common | compiler_current | stdlib_current) // Intentionally excluding `c` here, since this is a C++ library.
            #ifndef _MSC_VER
            // Only MSVC-like compilers print elaborated type specifiers.
            & ~bit_common_remove_type_prefix
            #endif
            ,
        // Only the rules that are relevant on the current compiler and standard library, with the assumption that the type names come from a method based on `__PRETTY_FUNCTION__`, `__FUNCSIG__`, `std::source_location::function_name()`.
        native_func_name_based_only = native
            #ifdef __GNUC__
            // GCC and Clang seem to remove default template arguments automatically.
            & ~bits_common_remove_defargs
            #endif
            ,
    };
    CPPDECL_FLAG_OPERATORS(SimplifyTypeNamesFlags)

    // A CRTP base.
    template <typename Derived>
    struct BasicSimplifyTypeNamesTraits
    {
        // This class should call all member functions of itself though those, to allow customization in derived classes.
        [[nodiscard]]       Derived &GetDerived()       {return static_cast<      Derived &>(*this);}
        [[nodiscard]] const Derived &GetDerived() const {return static_cast<const Derived &>(*this);}

        // Those are only used in this class:

        // If `name` is of the form `std[::{__cxx11,__1}]::A[::...]`, returns A. Otherwise returns an empty string.
        // Only permits the trailing `::...` if `index` isn't null.
        // On success, if `index` isn't null, writes the index of `A` into `*index`. That's typically 1, or 2 if we had to skip the version namespace.
        // On failure intentionally leaves `index` untouched.
        // Handling the version namespaces here is a bit redundant, but it allows us to do stuff independently from removing those namespaces, which is nice.
        [[nodiscard]] CPPDECL_CONSTEXPR std::string_view AsStdName(const QualifiedName &name, std::size_t *index = nullptr)
        {
            if (name.parts.size() < 2)
                return "";
            if (name.parts.front().AsSingleWord() != "std")
                return "";
            std::size_t part_index = 1;
            if (name.parts.at(1).AsSingleWord() == "__cxx11" || name.parts.at(1).AsSingleWord() == "__1")
                part_index++;
            if (index ? name.parts.size() < part_index + 1 : name.parts.size() != part_index + 1)
                return "";
            if (index)
                *index = part_index;

            // Not using `.AsSingleWord()` here to allow template arguments.
            if (auto unqual_name = std::get_if<std::string>(&name.parts.at(part_index).var))
                return *unqual_name;
            else
                return "";
        }
        // Same but for types.
        [[nodiscard]] CPPDECL_CONSTEXPR std::string_view AsStdName(const Type &type, std::size_t *index = nullptr)
        {
            return !type.IsEmpty() && type.IsOnlyQualifiedName() ? GetDerived().AsStdName(type.simple_type.name, index) : "";
        }


        // Those can be overridden:

        // Note that all those must leave `index` untouched if they return false.

        // `A<T, std::char_traits<T>, std::allocator<T>>`
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsStringLike(const QualifiedName &name, std::size_t *index)
        {
            // This has to be a separate variable because we don't want to write to `*index` if the string comparison is false,
            //   but `AsStdName()` has still succeeded.
            std::size_t std_index = 0;
            std::string_view std_name = GetDerived().AsStdName(name, &std_index);
            bool ok =
                std_name == "basic_string";

            if (ok && index)
                *index = std_index;
            return ok;
        }
        // `A<T, std::allocator<T>>`
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsVectorLike(const QualifiedName &name, std::size_t *index)
        {
            std::size_t std_index = 0;
            std::string_view std_name = GetDerived().AsStdName(name, &std_index);
            bool ok =
                std_name == "vector" ||
                std_name == "list" ||
                std_name == "forward_list" ||
                std_name == "deque" ||
                std_name == "hive";

            if (ok && index)
                *index = std_index;
            return ok;
        }
        // `A<T, std::less<T>, std::allocator<T>>`
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsOrderedSetLike(const QualifiedName &name, std::size_t *index)
        {
            std::size_t std_index = 0;
            std::string_view std_name = GetDerived().AsStdName(name, &std_index);
            bool ok =
                std_name == "set" ||
                std_name == "multiset";

            if (ok && index)
                *index = std_index;
            return ok;
        }
        // `A<T, U, std::less<T>, std::allocator<std::pair<const T, U>>>`
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsOrderedMapLike(const QualifiedName &name, std::size_t *index)
        {
            std::size_t std_index = 0;
            std::string_view std_name = GetDerived().AsStdName(name, &std_index);
            bool ok =
                std_name == "map" ||
                std_name == "multimap";

            if (ok && index)
                *index = std_index;
            return ok;
        }
        // `A<T, std::hash<T>, std::equal_to<T>, std::allocator<T>>`
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsUnorderedSetLike(const QualifiedName &name, std::size_t *index)
        {
            std::size_t std_index = 0;
            std::string_view std_name = GetDerived().AsStdName(name, &std_index);
            bool ok =
                std_name == "unordered_set" ||
                std_name == "unordered_multiset";

            if (ok && index)
                *index = std_index;
            return ok;
        }
        // `A<T, U, std::hash<T>, std::equal_to<T>, std::allocator<std::pair<const T, U>>>`
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsUnorderedMapLike(const QualifiedName &name, std::size_t *index)
        {
            std::size_t std_index = 0;
            std::string_view std_name = GetDerived().AsStdName(name, &std_index);
            bool ok =
                std_name == "unordered_map" ||
                std_name == "unordered_multimap";

            if (ok && index)
                *index = std_index;
            return ok;
        }
        // One of the various type that have char traits as the second template argument.
        // I got this list by searching for `preferred_name` (case-independent) in libc++ sources.
        [[nodiscard]] CPPDECL_CONSTEXPR bool HasCharTraits(const QualifiedName &name, std::size_t *index)
        {
            // This has to be a separate variable because we don't want to write to `*index` if the string comparison is false,
            //   but `AsStdName()` has still succeeded.
            std::size_t std_index = 0;
            std::string_view std_name = GetDerived().AsStdName(name, &std_index);
            bool ok =
                // We rely on all those having typedefs without `basic_` for specific character types.
                // This assumption is made in `SpecializationsHaveTypedefsForCharTypes`. If that's not the case for your types, the logic will have to be changed somehow.
                std_name == "basic_string" ||
                std_name == "basic_string_view" ||
                std_name == "basic_ios" ||
                std_name == "basic_filebuf" ||
                std_name == "basic_streambuf" ||
                std_name == "basic_stringbuf" ||
                std_name == "basic_istream" ||
                std_name == "basic_ostream" ||
                std_name == "basic_iostream" ||
                std_name == "basic_stringstream" ||
                std_name == "basic_istringstream" ||
                std_name == "basic_ostringstream" ||
                std_name == "basic_ostringstream" ||
                std_name == "basic_fstream" ||
                std_name == "basic_ifstream" ||
                std_name == "basic_ofstream";

            if (ok && index)
                *index = std_index;
            return ok;
        }
        // Whether this name can be shortened by removing the single template argument of a char type and replacing the `basic_` prefix (or some other?) with the abbreviation of the char type.
        // `resulting_string_base` on success receives the part of the name after `basic_`.
        // `allow_all_char_types` on success receives true if this type understands `char{8,16,32}_t` too, not just `char` and `wchar_t`.
        // The prefix to prepend to `resulting_string_base` is assumed to be always fixed.
        [[nodiscard]] CPPDECL_CONSTEXPR bool SpecializationsHaveTypedefsForCharTypes(const QualifiedName &name, std::size_t *index, std::string_view *resulting_string_base, bool *allow_all_char_types)
        {
            // This has to be a separate variable because we don't want to write to `*index` if the string comparison is false,
            //   but `AsStdName()` has still succeeded.
            std::size_t std_index = 0;

            // Intentionally no `GetDerived().` here.
            if (HasCharTraits(name, &std_index))
            {
                if (index)
                    *index = std_index;

                std::string_view name_word = name.parts.at(std_index).AsSingleWordIgnoringTemplateArgs();
                assert(name_word.starts_with("basic_")); // We assume that the name starts with `basic_`.
                if (resulting_string_base)
                    *resulting_string_base = name_word.substr(6); // Remove `basic_`.
                if (allow_all_char_types)
                    *allow_all_char_types = name_word == "basic_string" || name_word == "basic_string_view";

                return true;
            }

            return false;
        }


        // Is this a character traits type that we should remove from `IsStringLike()` types?
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsCharTraits(const Type &type)
        {
            return GetDerived().AsStdName(type) == "char_traits";
        }
        // Is this an allocator type that we can remove?
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsAllocator(const Type &type)
        {
            return GetDerived().AsStdName(type) == "allocator";
        }
        // Is this a pair type that will appear as the allocator parameter in maps?
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsPairInAllocatorParam(const Type &type)
        {
            return GetDerived().AsStdName(type) == "pair";
        }
        // Is this a less comparator that should be removed from ordered containers?
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsLessComparator(const Type &type)
        {
            return GetDerived().AsStdName(type) == "less";
        }
        // Is this an equality comparator that should be removed from unordered containers?
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsEqualToComparator(const Type &type)
        {
            return GetDerived().AsStdName(type) == "equal_to";
        }
        // Is this a hash functor that should be removed from unordered containers?
        [[nodiscard]] CPPDECL_CONSTEXPR bool IsHashFunctor(const Type &type)
        {
            return GetDerived().AsStdName(type) == "hash";
        }
    };
    struct DefaultSimplifyTypeNamesTraits : BasicSimplifyTypeNamesTraits<DefaultSimplifyTypeNamesTraits> {};

    // Simplify a name with the assumption that it's a type name.
    // This is a low-level function, prefer `SimplifyTypeNames()`.
    template <typename Traits = DefaultSimplifyTypeNamesTraits>
    CPPDECL_CONSTEXPR void SimplifyTypeQualifiedName(SimplifyTypeNamesFlags flags, QualifiedName &name, Traits &&traits = {})
    {
        // Rewrite `_Bool` as `bool`.
        if (bool(flags & SimplifyTypeNamesFlags::bit_c_normalize_bool) && name.AsSingleWord() == "_Bool")
        {
            name.parts.at(0).var = "bool";
            return; // Surely we don't need to check anything else.
        }

        // Remove the version namespace from std.
        if (bool(flags & (SimplifyTypeNamesFlags::bit_libstdcxx_remove_cxx11_namespace_in_std | SimplifyTypeNamesFlags::bit_libcpp_remove_1_namespace_in_std)))
        {
            // The first part of `name` is `std`, and there are at least two parts.
            bool is_in_std = name.parts.size() >= 2 && name.parts.front().AsSingleWord() == "std";

            bool removed_std_version_namespace = false;
            if (!removed_std_version_namespace && bool(flags & SimplifyTypeNamesFlags::bit_libstdcxx_remove_cxx11_namespace_in_std))
            {
                if (is_in_std && name.parts.at(1).AsSingleWord() == "__cxx11")
                {
                    removed_std_version_namespace = true;
                    name.parts.erase(name.parts.begin() + 1);
                }
            }
            if (!removed_std_version_namespace && bool(flags & SimplifyTypeNamesFlags::bit_libcpp_remove_1_namespace_in_std))
            {
                if (is_in_std && name.parts.at(1).AsSingleWord() == "__1")
                {
                    removed_std_version_namespace = true;
                    name.parts.erase(name.parts.begin() + 1);
                }
            }
        }

        { // Rewrite iterator names.
            bool already_normalized_iter = false;

            // libstdc++
            if (!already_normalized_iter && bool(flags & SimplifyTypeNamesFlags::bit_libstdcxx_normalize_iterators))
            {
                // std::vector
                if (
                    !already_normalized_iter &&
                    name.parts.size() >= 2 &&
                    name.parts.at(0).AsSingleWord() == "__gnu_cxx" &&
                    name.parts.at(1).AsSingleWordIgnoringTemplateArgs() == "__normal_iterator" &&
                    name.parts.at(1).template_args &&
                    name.parts.at(1).template_args->args.size() == 2
                )
                {
                    auto targ0 = std::get_if<Type>(&name.parts.at(1).template_args->args.at(0).var);
                    auto targ1 = std::get_if<Type>(&name.parts.at(1).template_args->args.at(1).var);
                    if (
                        targ0 &&
                        targ1 &&
                        targ0->Is<Pointer>() &&
                        traits.AsStdName(*targ1) == "vector" &&
                        targ1->simple_type.name.parts.back().template_args &&
                        targ1->simple_type.name.parts.back().template_args->args.size() >= 1
                    )
                    {

                        if (auto vector_targ0 = std::get_if<Type>(&targ1->simple_type.name.parts.back().template_args->args.at(0).var))
                        {
                            auto targ0_without_ptr = *targ0;
                            targ0_without_ptr.RemoveModifier().RemoveQualifiers(CvQualifiers::const_);
                            if (*vector_targ0 == targ0_without_ptr)
                            {
                                // Success!
                                already_normalized_iter = true;
                                bool is_const = targ0->IsConst(1);
                                QualifiedName container_name = std::move(targ1->simple_type.name);
                                name.parts.erase(name.parts.begin(), name.parts.begin() + 2);
                                name.parts.insert(name.parts.begin(), std::make_move_iterator(container_name.parts.begin()), std::make_move_iterator(container_name.parts.end()));
                                name.parts.emplace(name.parts.begin() + container_name.parts.size(), is_const ? "const_iterator" : "iterator");
                            }
                        }
                    }
                }

                // std::deque
                if (
                    !already_normalized_iter &&
                    name.parts.size() >= 2 &&
                    name.parts.at(0).AsSingleWord() == "std" && // No version namespace here, so it's easier to check manually, without using `traits.AsStdName()`.
                    name.parts.at(1).AsSingleWordIgnoringTemplateArgs() == "_Deque_iterator" &&
                    name.parts.at(1).template_args &&
                    name.parts.at(1).template_args->args.size() == 3
                )
                {
                    auto targ0 = std::get_if<Type>(&name.parts.at(1).template_args->args.at(0).var);
                    auto targ1 = std::get_if<Type>(&name.parts.at(1).template_args->args.at(1).var);
                    auto targ2 = std::get_if<Type>(&name.parts.at(1).template_args->args.at(2).var);
                    if (
                        targ0 &&
                        targ1 &&
                        targ2 &&
                        targ1->Is<Reference>() &&
                        targ2->Is<Pointer>()
                    )
                    {
                        // Somehow checking `targ2` (a pointer) feels a bit more reliable than checking `targ1` (a reference).
                        bool is_const = targ2->IsConst(1);

                        // Since the element type of `std::deque` can't be const, testing this way is fine.

                        auto targ1_without_const_ref = *targ1;
                        targ1_without_const_ref.RemoveModifier().RemoveQualifiers(CvQualifiers::const_);

                        auto targ2_without_const_ptr = *targ2;
                        targ2_without_const_ptr.RemoveModifier().RemoveQualifiers(CvQualifiers::const_);

                        if (*targ0 == targ1_without_const_ref && *targ0 == targ2_without_const_ptr)
                        {
                            // Success!
                            already_normalized_iter = true;
                            name.parts.at(1).var = "deque";
                            name.parts.at(1).template_args->args.resize(1);
                            name.parts.emplace(name.parts.begin() + 2, is_const ? "const_iterator" : "iterator");
                        }
                    }
                }

                // std::forward_list
                if (
                    !already_normalized_iter &&
                    name.parts.size() >= 2 &&
                    name.parts.at(0).AsSingleWord() == "std" // No version namespace here, so it's easier to check manually, without using `traits.AsStdName()`.
                )
                {
                    bool is_mut = name.parts.at(1).AsSingleWordIgnoringTemplateArgs() == "_Fwd_list_iterator";
                    bool is_const = !is_mut && name.parts.at(1).AsSingleWordIgnoringTemplateArgs() == "_Fwd_list_const_iterator";

                    if (
                        (is_mut || is_const) &&
                        name.parts.at(1).template_args &&
                        name.parts.at(1).template_args->args.size() == 1
                    )
                    {
                        // Success!
                        already_normalized_iter = true;
                        name.parts.at(1).var = "forward_list";
                        name.parts.emplace(name.parts.begin() + 2, is_const ? "const_iterator" : "iterator");
                    }
                }

                // std::list
                if (
                    !already_normalized_iter &&
                    name.parts.size() >= 2 &&
                    name.parts.at(0).AsSingleWord() == "std" // No version namespace here, so it's easier to check manually, without using `traits.AsStdName()`.
                )
                {
                    bool is_mut = name.parts.at(1).AsSingleWordIgnoringTemplateArgs() == "_List_iterator";
                    bool is_const = !is_mut && name.parts.at(1).AsSingleWordIgnoringTemplateArgs() == "_List_const_iterator";

                    if (
                        (is_mut || is_const) &&
                        name.parts.at(1).template_args &&
                        name.parts.at(1).template_args->args.size() == 1
                    )
                    {
                        // Success!
                        already_normalized_iter = true;
                        name.parts.at(1).var = "list";
                        name.parts.emplace(name.parts.begin() + 2, is_const ? "const_iterator" : "iterator");
                    }
                }

                // std::set, std::multiset, std::map, std::multimap
                if (
                    !already_normalized_iter &&
                    name.parts.size() >= 2 &&
                    name.parts.at(0).AsSingleWord() == "std" // No version namespace here, so it's easier to check manually, without using `traits.AsStdName()`.
                )
                {
                    bool is_mut = name.parts.at(1).AsSingleWordIgnoringTemplateArgs() == "_Rb_tree_iterator";
                    bool is_const = !is_mut && name.parts.at(1).AsSingleWordIgnoringTemplateArgs() == "_Rb_tree_const_iterator";

                    if (
                        (is_mut || is_const) &&
                        name.parts.at(1).template_args &&
                        name.parts.at(1).template_args->args.size() == 1
                    )
                    {
                        if (auto targ = std::get_if<Type>(&name.parts.at(1).template_args->args.at(0).var))
                        {
                            // This gets set if this is a map as opposed to a set, and receives the list of its arguments.
                            TemplateArgumentList *map_targs_ptr = nullptr;
                            if (
                                targ->IsOnlyQualifiedName() &&
                                targ->simple_type.name.parts.size() == 2 &&
                                targ->simple_type.name.parts.at(0).AsSingleWord() == "std" && // No version namespace here, so it's easier to check manually, without using `traits.AsStdName()`.
                                targ->simple_type.name.parts.at(1).AsSingleWordIgnoringTemplateArgs() == "pair" &&
                                targ->simple_type.name.parts.at(1).template_args &&
                                targ->simple_type.name.parts.at(1).template_args->args.size() == 2
                            )
                            {
                                auto targ0 = std::get_if<Type>(&targ->simple_type.name.parts.at(1).template_args->args.at(0).var);
                                auto targ1 = std::get_if<Type>(&targ->simple_type.name.parts.at(1).template_args->args.at(1).var);
                                if (
                                    targ0 &&
                                    targ1 &&
                                    targ0->IsConst()
                                    // Not checking `!targ1->IsConst()` because apparently maps can have non-const values just fine. Tested on libstdc++, libc++, and MSVC STL.
                                )
                                {
                                    // Can't adjust `map_targs_ptr` here yet, because there are more conditions to check below.
                                    map_targs_ptr = &*targ->simple_type.name.parts.at(1).template_args;
                                }
                            }

                            // Reject if this is a non-const iterator and at the same time not a map iterator.
                            // This could never happen in my tests (because the set iterators are always const).
                            if (is_const || map_targs_ptr)
                            {
                                // Success!
                                already_normalized_iter = true;

                                name.parts.at(1).var = map_targs_ptr ? "map" : "set";
                                if (map_targs_ptr)
                                {
                                    // We introduce a temporary variable to avoid directly moving the targs into their parent, which sounds unsafe.
                                    TemplateArgumentList map_targs = std::move(*map_targs_ptr);
                                    std::get<Type>(map_targs.args.at(0).var).RemoveQualifiers(CvQualifiers::const_);
                                    name.parts.at(1).template_args = std::move(map_targs);
                                }
                                name.parts.emplace(name.parts.begin() + 2, is_const ? "const_iterator" : "iterator");
                            }
                        }
                    }
                }

                // std::unordered_set, std::unordered_multiset, std::unordered_map, std::unordered_multimap
                if (
                    !already_normalized_iter &&
                    name.parts.size() >= 3 &&
                    name.parts.at(0).AsSingleWord() == "std" && // No version namespace here, so it's easier to check manually, without using `traits.AsStdName()`.
                    name.parts.at(1).AsSingleWord() == "__detail" // No version namespace here, so it's easier to check manually, without using `traits.AsStdName()`.
                )
                {
                    bool is_mut = name.parts.at(2).AsSingleWordIgnoringTemplateArgs() == "_Node_iterator";
                    bool is_const = !is_mut && name.parts.at(2).AsSingleWordIgnoringTemplateArgs() == "_Node_const_iterator";

                    if (
                        (is_mut || is_const) &&
                        name.parts.at(2).template_args &&
                        name.parts.at(2).template_args->args.size() == 3
                    )
                    {
                        auto targ0 = std::get_if<Type>(&name.parts.at(2).template_args->args.at(0).var);
                        auto targ1 = std::get_if<PseudoExpr>(&name.parts.at(2).template_args->args.at(1).var);
                        auto targ2 = std::get_if<PseudoExpr>(&name.parts.at(2).template_args->args.at(2).var);

                        if (
                            targ0 &&
                            targ1 &&
                            targ2 &&
                            targ1->tokens.size() == 1 &&
                            targ2->tokens.size() == 1
                        )
                        {
                            auto targ1_token = std::get_if<SimpleType>(&targ1->tokens.front());
                            auto targ2_token = std::get_if<SimpleType>(&targ2->tokens.front());

                            if (
                                targ1_token &&
                                targ2_token &&
                                targ2_token->AsSingleWord() == "false"
                            )
                            {
                                bool is_set = targ1_token->AsSingleWord() == "true";
                                bool is_map = !is_set && targ1_token->AsSingleWord() == "false";

                                if (is_set || is_map)
                                {
                                    TemplateArgumentList *map_targs_ptr = nullptr;
                                    if (
                                        is_map &&
                                        targ0->IsOnlyQualifiedName() &&
                                        targ0->simple_type.name.parts.size() == 2 &&
                                        targ0->simple_type.name.parts.at(0).AsSingleWord() == "std" && // No version namespace here, so it's easier to check manually, without using `traits.AsStdName()`.
                                        targ0->simple_type.name.parts.at(1).AsSingleWordIgnoringTemplateArgs() == "pair" &&
                                        targ0->simple_type.name.parts.at(1).template_args &&
                                        targ0->simple_type.name.parts.at(1).template_args->args.size() == 2
                                    )
                                    {
                                        auto pair_targ0 = std::get_if<Type>(&targ0->simple_type.name.parts.at(1).template_args->args.at(0).var);
                                        auto pair_targ1 = std::get_if<Type>(&targ0->simple_type.name.parts.at(1).template_args->args.at(1).var);
                                        if (
                                            pair_targ0 &&
                                            pair_targ1 &&
                                            pair_targ0->IsConst()
                                            // Not checking `!targ1->IsConst()` because apparently maps can have non-const values just fine. Tested on libstdc++, libc++, and MSVC STL.
                                        )
                                        {
                                            // Can't adjust `map_targs_ptr` here yet, because there are more conditions to check below.
                                            map_targs_ptr = &*targ0->simple_type.name.parts.at(1).template_args;
                                        }
                                    }

                                    if (is_map == bool(map_targs_ptr))
                                    {
                                        // Success!
                                        already_normalized_iter = true;

                                        name.parts.at(1).var = is_map ? "unordered_map" : "unordered_set";
                                        if (is_map)
                                        {
                                            name.parts.at(1).template_args = std::move(*map_targs_ptr);
                                            std::get<Type>(name.parts.at(1).template_args->args.at(0).var).RemoveQualifiers(CvQualifiers::const_);
                                            name.parts.at(2).template_args.reset();
                                        }
                                        else
                                        {
                                            name.parts.at(1).template_args = std::move(name.parts.at(2).template_args);
                                            name.parts.at(1).template_args->args.resize(1);
                                            name.parts.at(2).template_args.reset();
                                        }
                                        name.parts.at(2).var = is_const ? "const_iterator" : "iterator";
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // libc++
            if (!already_normalized_iter && bool(flags & SimplifyTypeNamesFlags::bit_libcpp_normalize_iterators))
            {
                // Here we don't use `traits.AsStdName()` because we only need to support one specific spelling of the version namespace.

                if (name.parts.size() >= 2 && name.parts.at(0).AsSingleWord() == "std")
                {
                    const std::size_t part_index = name.parts.at(1).AsSingleWord() == "__1" ? 2 : 1;

                    if (
                        part_index < name.parts.size() &&
                        name.parts.at(part_index).IsSingleWordWordIgnoringTemplateArgs() &&
                        name.parts.at(part_index).template_args
                    )
                    {
                        UnqualifiedName &part = name.parts.at(part_index);
                        const std::string_view word = part.AsSingleWordIgnoringTemplateArgs();

                        auto CountsAsPtrdiffType = [](const Type &type) -> bool
                        {
                            std::string_view word = type.AsSingleWord();
                            // Could later make those checks platform-specific if needed.
                            return word == "long" || word == "long long";
                        };
                        auto CountsAsPtrdiffConstant = [](const PseudoExpr &expr, std::string_view value) -> bool
                        {
                            if (expr.tokens.size() != 1)
                                return false;
                            auto number = std::get_if<NumberToken>(&expr.tokens.front());
                            if (!number)
                                return false;
                            if (!number->value.starts_with(value))
                                return false;
                            // If we change `NumberToken` to actually parse the type suffixes, this will need to be rewritten.
                            std::string_view view = number->value;
                            view.remove_prefix(value.size());
                            // Could later make those checks platform-specific if needed.
                            return view.empty() || view == "l" || view == "L" || view == "ll" || view == "LL";
                        };
                        auto IsLibcppStdNameIgnoringTemplateArgs = [](const QualifiedName &name, std::string_view target) -> bool
                        {
                            // Not using `traits.AsStdName()` because this only needs one specific version namespace, the `__1`.
                            // We're also not using this lambda for the first check above, because here we don't allow more unqualified names after the `target`.
                            if (name.parts.size() < 2)
                                return false;
                            if (name.parts.at(0).AsSingleWord() != "std")
                                return false;
                            bool has_version_namespace = name.parts.at(1).AsSingleWord() == "__1";
                            if (name.parts.size() != (has_version_namespace ? 3 : 2))
                                return false;
                            return name.parts.back().AsSingleWordIgnoringTemplateArgs() == target;
                        };

                        // std::vector
                        if (
                            !already_normalized_iter &&
                            word == "__wrap_iter" &&
                            part.template_args->args.size() == 1
                        )
                        {
                            if (
                                auto type = std::get_if<Type>(&part.template_args->args.at(0).var);
                                type &&
                                type->Is<Pointer>()
                            )
                            {
                                // Success!
                                already_normalized_iter = true;

                                const bool is_const = type->IsConst(1);
                                part.var = "vector";
                                name.parts.emplace(name.parts.begin() + part_index + 1, is_const ? "const_iterator" : "iterator");
                                type->RemoveModifier().RemoveQualifiers(CvQualifiers::const_);
                            }
                        }

                        // std::deque
                        if (
                            !already_normalized_iter &&
                            word == "__deque_iterator" &&
                            part.template_args->args.size() == 6
                        )
                        {
                            auto targ0 = std::get_if<Type>(&part.template_args->args.at(0).var);
                            auto targ1 = std::get_if<Type>(&part.template_args->args.at(1).var);
                            auto targ2 = std::get_if<Type>(&part.template_args->args.at(2).var);
                            auto targ3 = std::get_if<Type>(&part.template_args->args.at(3).var);
                            auto targ4 = std::get_if<Type>(&part.template_args->args.at(4).var);
                            auto targ5 = std::get_if<PseudoExpr>(&part.template_args->args.at(5).var);
                            if (
                                targ0 &&
                                targ1 && targ1->Is<Pointer>() &&
                                targ2 && targ2->Is<Reference>() &&
                                targ3 && targ3->Is<Pointer>() && targ3->Is<Pointer>(1) && // `Is()` correctly handles the index being out of bounds by returning false.
                                targ4 && CountsAsPtrdiffType(*targ4) &&
                                targ5 && CountsAsPtrdiffConstant(*targ5, "1024")
                            )
                            {
                                const bool is_const = targ1->IsConst(1);
                                if (
                                    targ2->IsConst(1) == is_const &&
                                    targ3->IsConst(1) == is_const && targ3->IsConst(2) == is_const // Both pointer levels have the same constness.
                                )
                                {
                                    auto targ1_without_const_ptr = *targ1;
                                    auto targ2_without_const_ref = *targ2;
                                    auto targ3_without_double_const_ref = *targ3;
                                    targ1_without_const_ptr.RemoveModifier().RemoveQualifiers(CvQualifiers::const_);
                                    targ2_without_const_ref.RemoveModifier().RemoveQualifiers(CvQualifiers::const_);
                                    targ3_without_double_const_ref.RemoveModifier().RemoveModifier().RemoveQualifiers(CvQualifiers::const_);

                                    if (
                                        *targ0 == targ1_without_const_ptr &&
                                        *targ0 == targ2_without_const_ref &&
                                        *targ0 == targ3_without_double_const_ref
                                    )
                                    {
                                        // Success!
                                        already_normalized_iter = true;

                                        part.var = "deque";
                                        part.template_args->args.resize(1);
                                        name.parts.emplace(name.parts.begin() + part_index + 1, is_const ? "const_iterator" : "iterator");
                                    }
                                }
                            }
                        }

                        // std::forward_list
                        if (
                            !already_normalized_iter &&
                            part.template_args->args.size() == 1
                        )
                        {
                            const bool is_mut = word == "__forward_list_iterator";
                            const bool is_const = !is_mut && word == "__forward_list_const_iterator";
                            if (is_mut || is_const)
                            {
                                if (
                                    auto targ = std::get_if<Type>(&part.template_args->args.at(0).var);
                                    targ &&
                                    targ->Is<Pointer>() &&
                                    targ->GetQualifiers() == CvQualifiers{} &&
                                    targ->modifiers.size() == 1 &&
                                    targ->simple_type.IsOnlyQualifiedName() &&
                                    IsLibcppStdNameIgnoringTemplateArgs(targ->simple_type.name, "__forward_list_node") &&
                                    targ->simple_type.name.parts.back().template_args &&
                                    targ->simple_type.name.parts.back().template_args->args.size() == 2
                                )
                                {
                                    auto targ0 = std::get_if<Type>(&targ->simple_type.name.parts.back().template_args->args.at(0).var);
                                    auto targ1 = std::get_if<Type>(&targ->simple_type.name.parts.back().template_args->args.at(1).var);
                                    if (
                                        targ0 &&
                                        targ1 &&
                                        *targ1 == Type::FromSingleWord("void").AddModifier(Pointer{})
                                    )
                                    {
                                        // Success!
                                        already_normalized_iter = true;

                                        part.var = "forward_list";

                                        // Move into a temporary variable first. Moving directly to parent seems unsafe.
                                        Type elem_type = std::move(*targ0);
                                        part.template_args->args.resize(1);
                                        part.template_args->args.front().var = std::move(elem_type);

                                        name.parts.emplace(name.parts.begin() + part_index + 1, is_const ? "const_iterator" : "iterator");
                                    }
                                }
                            }
                        }

                        // std::list
                        if (
                            !already_normalized_iter &&
                            part.template_args->args.size() == 2
                        )
                        {
                            const bool is_mut = word == "__list_iterator";
                            const bool is_const = !is_mut && word == "__list_const_iterator";
                            if (is_mut || is_const)
                            {
                                auto targ0 = std::get_if<Type>(&part.template_args->args.at(0).var);
                                auto targ1 = std::get_if<Type>(&part.template_args->args.at(1).var);
                                if (
                                    targ0 &&
                                    targ1 &&
                                    *targ1 == Type::FromSingleWord("void").AddModifier(Pointer{})
                                )
                                {
                                    // Success!
                                    already_normalized_iter = true;

                                    part.var = "list";
                                    part.template_args->args.resize(1);
                                    name.parts.emplace(name.parts.begin() + part_index + 1, is_const ? "const_iterator" : "iterator");
                                }
                            }
                        }

                        // Returns true if `part`if those template arguments look like those if a `std::set` iterator.
                        // This is in a lambda because we need to reuse it for `std::map` iterators.
                        auto IsSetIteratorTargs = [&CountsAsPtrdiffType, &IsLibcppStdNameIgnoringTemplateArgs](const std::optional<TemplateArgumentList> &targs) -> bool
                        {
                            if (
                                targs &&
                                targs->args.size() == 3
                            )
                            {
                                auto targ0 = std::get_if<Type>(&targs->args.at(0).var);
                                auto targ1 = std::get_if<Type>(&targs->args.at(1).var);
                                auto targ2 = std::get_if<Type>(&targs->args.at(2).var);
                                if (
                                    targ0 &&
                                    targ1 &&
                                    targ1->Is<Pointer>() &&
                                    targ1->GetQualifiers() == CvQualifiers{} &&
                                    targ1->modifiers.size() == 1 &&
                                    targ1->simple_type.IsOnlyQualifiedName() &&
                                    IsLibcppStdNameIgnoringTemplateArgs(targ1->simple_type.name, "__tree_node") &&
                                    targ1->simple_type.name.parts.back().template_args &&
                                    targ1->simple_type.name.parts.back().template_args->args.size() == 2 &&
                                    targ2 &&
                                    CountsAsPtrdiffType(*targ2)
                                )
                                {
                                    auto sub_targ0 = std::get_if<Type>(&targ1->simple_type.name.parts.back().template_args->args.at(0).var);
                                    auto sub_targ1 = std::get_if<Type>(&targ1->simple_type.name.parts.back().template_args->args.at(1).var);
                                    if (
                                        sub_targ0 &&
                                        sub_targ1 &&
                                        *sub_targ1 == Type::FromSingleWord("void").AddModifier(Pointer{}) &&
                                        // Check this last, because this is potentially more expensive for big types.
                                        *sub_targ0 == *targ0
                                    )
                                    {
                                        return true;
                                    }
                                }
                            }

                            return false;
                        };

                        // std::set
                        if (
                            !already_normalized_iter &&
                            word == "__tree_const_iterator" &&
                            IsSetIteratorTargs(part.template_args)
                        )
                        {
                            // We already checked that that template argument is a type (in IsSetIteratorTargs()`), so we can `std::get()` directly here.
                            const Type &type = std::get<Type>(part.template_args->args.front().var);
                            // Make sure we exclude `std::map` iterator internals.
                            if (!(type.IsOnlyQualifiedName() && IsLibcppStdNameIgnoringTemplateArgs(type.simple_type.name, "__value_type")))
                            {
                                // Success!
                                already_normalized_iter = true;

                                part.var = "set";
                                part.template_args->args.resize(1);
                                name.parts.emplace(name.parts.begin() + part_index + 1, "const_iterator");
                            }
                        }

                        // std::map
                        if (
                            !already_normalized_iter &&
                            part.template_args->args.size() == 1
                        )
                        {
                            const bool is_mut = word == "__map_iterator";
                            const bool is_const = !is_mut && word == "__map_const_iterator";
                            if (is_mut || is_const)
                            {
                                if (
                                    auto targ = std::get_if<Type>(&part.template_args->args.front().var);
                                    targ &&
                                    targ->IsOnlyQualifiedName() &&
                                    IsLibcppStdNameIgnoringTemplateArgs(targ->simple_type.name, is_const ? "__tree_const_iterator" : "__tree_iterator") &&
                                    IsSetIteratorTargs(targ->simple_type.name.parts.back().template_args)
                                )
                                {
                                    if (
                                        auto sub_targ = std::get_if<Type>(&targ->simple_type.name.parts.back().template_args->args.front().var);
                                        sub_targ &&
                                        sub_targ->IsOnlyQualifiedName() &&
                                        IsLibcppStdNameIgnoringTemplateArgs(sub_targ->simple_type.name, "__value_type") &&
                                        sub_targ->simple_type.name.parts.back().template_args &&
                                        sub_targ->simple_type.name.parts.back().template_args->args.size() == 2 &&
                                        std::holds_alternative<Type>(sub_targ->simple_type.name.parts.back().template_args->args.at(0).var) &&
                                        std::holds_alternative<Type>(sub_targ->simple_type.name.parts.back().template_args->args.at(1).var)
                                    )
                                    {
                                        // Success!
                                        already_normalized_iter = true;

                                        part.var = "map";

                                        // Move into a temporary variable, because moving directly to the parent looks unsafe.
                                        TemplateArgumentList elem_types = std::move(*sub_targ->simple_type.name.parts.back().template_args);
                                        part.template_args = std::move(elem_types);

                                        name.parts.emplace(name.parts.begin() + part_index + 1, is_const ? "const_iterator" : "iterator");
                                    }
                                }
                            }
                        }

                        // Returns true if `part`if those template arguments look like those if a `std::unordered_set` iterator.
                        // This is in a lambda because we need to reuse it for `std::unordered_map` iterators.
                        auto IsUnorderedSetIteratorTargs = [&IsLibcppStdNameIgnoringTemplateArgs](const std::optional<TemplateArgumentList> &targs) -> bool
                        {
                            if (
                                targs &&
                                targs->args.size() == 1
                            )
                            {
                                auto targ0 = std::get_if<Type>(&targs->args.at(0).var);
                                if (
                                    targ0 &&
                                    targ0->Is<Pointer>() &&
                                    targ0->GetQualifiers() == CvQualifiers{} &&
                                    targ0->modifiers.size() == 1 &&
                                    targ0->simple_type.IsOnlyQualifiedName() &&
                                    IsLibcppStdNameIgnoringTemplateArgs(targ0->simple_type.name, "__hash_node") &&
                                    targ0->simple_type.name.parts.back().template_args &&
                                    targ0->simple_type.name.parts.back().template_args->args.size() == 2
                                )
                                {
                                    auto sub_targ0 = std::get_if<Type>(&targ0->simple_type.name.parts.back().template_args->args.at(0).var);
                                    auto sub_targ1 = std::get_if<Type>(&targ0->simple_type.name.parts.back().template_args->args.at(1).var);
                                    if (
                                        sub_targ0 &&
                                        sub_targ1 &&
                                        *sub_targ1 == Type::FromSingleWord("void").AddModifier(Pointer{})
                                    )
                                    {
                                        return true;
                                    }
                                }
                            }

                            return false;
                        };

                        // std::unordered_set
                        if (
                            !already_normalized_iter &&
                            word == "__hash_const_iterator" &&
                            IsUnorderedSetIteratorTargs(part.template_args)
                        )
                        {
                            // We already checked the preconditions for those two `std::get()` above in `IsUnorderedSetIteratorTargs()`, so calling `std::get()` here directly should be fine.
                            Type &type = std::get<Type>(std::get<Type>(part.template_args->args.front().var).simple_type.name.parts.back().template_args->args.front().var);
                            // Make sure we exclude `std::unordered_map` iterator internals.
                            if (!(type.IsOnlyQualifiedName() && IsLibcppStdNameIgnoringTemplateArgs(type.simple_type.name, "__hash_value_type")))
                            {
                                // Success!
                                already_normalized_iter = true;

                                part.var = "unordered_set";

                                // Move into a temporary variable, because moving directly into its parent seems unsafe.
                                Type elem_type = std::move(type);
                                part.template_args->args.at(0).var = std::move(elem_type);
                                name.parts.emplace(name.parts.begin() + part_index + 1, "const_iterator");
                            }
                        }

                        // std::map
                        if (
                            !already_normalized_iter &&
                            part.template_args->args.size() == 1
                        )
                        {
                            const bool is_mut = word == "__hash_map_iterator";
                            const bool is_const = !is_mut && word == "__hash_map_const_iterator";
                            if (is_mut || is_const)
                            {
                                if (
                                    auto targ = std::get_if<Type>(&part.template_args->args.front().var);
                                    targ &&
                                    targ->IsOnlyQualifiedName() &&
                                    IsLibcppStdNameIgnoringTemplateArgs(targ->simple_type.name, is_const ? "__hash_const_iterator" : "__hash_iterator") &&
                                    IsUnorderedSetIteratorTargs(targ->simple_type.name.parts.back().template_args)
                                )
                                {
                                    if (
                                        // Here we go TWO template arguments deep.
                                        // Without too many checks, since `IsUnorderedSetIteratorTargs()` should have already validated the structure here.
                                        auto sub_targ = std::get_if<Type>(&std::get<Type>(targ->simple_type.name.parts.back().template_args->args.front().var).simple_type.name.parts.back().template_args->args.front().var);
                                        sub_targ &&
                                        sub_targ->IsOnlyQualifiedName() &&
                                        IsLibcppStdNameIgnoringTemplateArgs(sub_targ->simple_type.name, "__hash_value_type") &&
                                        sub_targ->simple_type.name.parts.back().template_args &&
                                        sub_targ->simple_type.name.parts.back().template_args->args.size() == 2 &&
                                        std::holds_alternative<Type>(sub_targ->simple_type.name.parts.back().template_args->args.at(0).var) &&
                                        std::holds_alternative<Type>(sub_targ->simple_type.name.parts.back().template_args->args.at(1).var)
                                    )
                                    {
                                        // Success!
                                        already_normalized_iter = true;

                                        part.var = "unordered_map";

                                        // Move into a temporary variable, because moving directly to the parent looks unsafe.
                                        TemplateArgumentList elem_types = std::move(*sub_targ->simple_type.name.parts.back().template_args);
                                        part.template_args = std::move(elem_types);

                                        name.parts.emplace(name.parts.begin() + part_index + 1, is_const ? "const_iterator" : "iterator");
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Those need to be in a specific order, since we can only remove the last template argument at the every step:

        // Remove the allocator.
        if (bool(flags & SimplifyTypeNamesFlags::bit_common_remove_defarg_allocator))
        {
            std::size_t name_index = std::size_t(-1);

            const bool is_string_like        = traits.IsStringLike(name, &name_index);
            const bool is_vector_like        = traits.IsVectorLike(name, &name_index);
            const bool is_ordered_set_like   = traits.IsOrderedSetLike(name, &name_index);
            const bool is_ordered_map_like   = traits.IsOrderedMapLike(name, &name_index);
            const bool is_unordered_set_like = traits.IsUnorderedSetLike(name, &name_index);
            const bool is_unordered_map_like = traits.IsUnorderedMapLike(name, &name_index);

            if (
                is_string_like ||
                is_vector_like ||
                is_ordered_set_like ||
                is_ordered_map_like ||
                is_unordered_set_like ||
                is_unordered_map_like
            )
            {
                std::size_t allocator_targ_pos = std::size_t(-2); // `-2` is used to not enter the `if` below if none of the branches is taken and the assert is disabled.
                if (is_string_like)
                    allocator_targ_pos = 2; // After `std::char_traits`.
                else if (is_vector_like)
                    allocator_targ_pos = 1; // As usual.
                else if (is_ordered_set_like)
                    allocator_targ_pos = 2; // After comparator.
                else if (is_ordered_map_like)
                    allocator_targ_pos = 3; // After mapped type and comparator.
                else if (is_unordered_set_like)
                    allocator_targ_pos = 3; // After hash and equality.
                else if (is_unordered_map_like)
                    allocator_targ_pos = 4; // After mapped type, hash and equality.
                else
                    assert(false && "This should be unreachable.");

                const bool allocator_is_map_like = is_ordered_map_like || is_unordered_map_like;

                UnqualifiedName &name_part = name.parts.at(name_index);

                // The allocator must be the last argument, otherwise removing it will mess up the order.
                if (name_part.template_args && name_part.template_args->args.size() == allocator_targ_pos + 1)
                {
                    if (auto allocator_type = std::get_if<Type>(&name_part.template_args->args.back().var))
                    {
                        if (
                            traits.IsAllocator(*allocator_type) &&
                            allocator_type->simple_type.name.parts.back().template_args &&
                            allocator_type->simple_type.name.parts.back().template_args->args.size() == 1
                        )
                        {
                            if (auto allocator_targ = std::get_if<Type>(&allocator_type->simple_type.name.parts.back().template_args->args.front().var))
                            {
                                bool ok = false;
                                if (!allocator_is_map_like)
                                {
                                    if (auto our_targ = std::get_if<Type>(&name_part.template_args->args.at(0).var))
                                    {
                                        if (*our_targ == *allocator_targ)
                                            ok = true;
                                    }
                                }
                                else
                                {
                                    auto our_targ0 = std::get_if<Type>(&name_part.template_args->args.at(0).var);
                                    auto our_targ1 = std::get_if<Type>(&name_part.template_args->args.at(1).var);
                                    if (
                                        our_targ0 && our_targ1 &&
                                        traits.IsPairInAllocatorParam(*allocator_targ) &&
                                        allocator_targ->simple_type.name.parts.back().template_args &&
                                        allocator_targ->simple_type.name.parts.back().template_args->args.size() == 2
                                    )
                                    {
                                        auto allocator_targ0 = std::get_if<Type>(&allocator_targ->simple_type.name.parts.back().template_args->args.at(0).var);
                                        auto allocator_targ1 = std::get_if<Type>(&allocator_targ->simple_type.name.parts.back().template_args->args.at(1).var);
                                        if (
                                            allocator_targ0 && allocator_targ1 &&
                                            allocator_targ0->IsConst() &&
                                            *allocator_targ1 == *our_targ1 && // 0th one differs in constness, so we compare it below.
                                            our_targ0->GetQualifiersMut() // Make sure we can add qualifiers freely.
                                            )
                                        {
                                            // Temporarily add constness to the first template argument of our template, to simplify the comparison.
                                            struct ConstGuard
                                            {
                                                Type *our_targ0;
                                                bool added_constness = false;
                                                ConstGuard(Type *our_targ0)
                                                    : our_targ0(our_targ0)
                                                {
                                                    if (!our_targ0->IsConst())
                                                    {
                                                        added_constness = true;
                                                        our_targ0->AddQualifiers(CvQualifiers::const_);
                                                    }
                                                }
                                                ~ConstGuard()
                                                {
                                                    our_targ0->RemoveQualifiers(CvQualifiers::const_);
                                                }
                                            };
                                            ConstGuard const_guard(our_targ0);

                                            if (*allocator_targ0 == *our_targ0)
                                            {
                                                ok = true; // That's about it?
                                            }
                                        }
                                    }
                                }


                                // Lastly, if the template argument of the allocator matches, remove the allocator.
                                if (ok)
                                    name_part.template_args->args.pop_back();
                            }
                        }
                    }
                }
            }
        }

        // Remove char traits. Must be after removing the allocator.
        if (bool(flags & SimplifyTypeNamesFlags::bit_common_remove_defarg_char_traits))
        {
            std::size_t name_index = std::size_t(-1);

            if (traits.HasCharTraits(name, &name_index))
            {
                UnqualifiedName &name_part = name.parts.at(name_index);

                if (name_part.template_args && name_part.template_args->args.size() == 2)
                {
                    auto targ0 = std::get_if<Type>(&name_part.template_args->args.at(0).var);
                    auto targ1 = std::get_if<Type>(&name_part.template_args->args.at(1).var);
                    if (
                        targ0 && targ1 &&
                        traits.IsCharTraits(*targ1) &&
                        targ1->simple_type.name.parts.back().template_args &&
                        targ1->simple_type.name.parts.back().template_args->args.size() == 1
                    )
                    {
                        if (auto allocator_targ = std::get_if<Type>(&targ1->simple_type.name.parts.back().template_args->args.front().var))
                        {
                            if (*allocator_targ == *targ0)
                            {
                                // Success!
                                name_part.template_args->args.pop_back();
                            }
                        }
                    }
                }
            }
        }

        // Remove `std::less` and `std::equal_to`. Must be after removing the allocator.
        if (bool(flags & SimplifyTypeNamesFlags::bit_common_remove_defarg_comparator))
        {
            std::size_t name_index = std::size_t(-1);

            const bool is_ordered_set_like   = traits.IsOrderedSetLike(name, &name_index);
            const bool is_ordered_map_like   = traits.IsOrderedMapLike(name, &name_index);
            const bool is_unordered_set_like = traits.IsUnorderedSetLike(name, &name_index);
            const bool is_unordered_map_like = traits.IsUnorderedMapLike(name, &name_index);

            if (
                is_ordered_set_like ||
                is_ordered_map_like ||
                is_unordered_set_like ||
                is_unordered_map_like
            )
            {
                std::size_t comparator_targ_pos = std::size_t(-2); // `-2` is used to not enter the `if` below if none of the branches is taken and the assert is disabled.
                if (is_ordered_set_like)
                    comparator_targ_pos = 1; // Right after the element type.
                else if (is_ordered_map_like)
                    comparator_targ_pos = 2; // After the mapped type.
                else if (is_unordered_set_like)
                    comparator_targ_pos = 2; // After the hash functor.
                else if (is_unordered_map_like)
                    comparator_targ_pos = 3; // After the mapped type and the hash.
                else
                    assert(false && "This should be unreachable.");

                const bool container_is_unordered = is_unordered_set_like || is_unordered_map_like;

                UnqualifiedName &name_part = name.parts.at(name_index);

                // The comparator must be the last argument, otherwise removing it will mess up the order.
                if (name_part.template_args && name_part.template_args->args.size() == comparator_targ_pos + 1)
                {
                    if (auto comparator_type = std::get_if<Type>(&name_part.template_args->args.back().var))
                    {
                        if (
                            (container_is_unordered ? traits.IsEqualToComparator(*comparator_type) : traits.IsLessComparator(*comparator_type)) &&
                            comparator_type->simple_type.name.parts.back().template_args &&
                            comparator_type->simple_type.name.parts.back().template_args->args.size() == 1
                        )
                        {
                            if (auto comparator_targ = std::get_if<Type>(&comparator_type->simple_type.name.parts.back().template_args->args.front().var))
                            {
                                if (auto our_targ = std::get_if<Type>(&name_part.template_args->args.at(0).var))
                                {
                                    if (*our_targ == *comparator_targ)
                                        name_part.template_args->args.pop_back();
                                }
                            }
                        }
                    }
                }
            }
        }

        // Remove `std::hash`. Must be after removing the comparator (and the allocator).
        if (bool(flags & SimplifyTypeNamesFlags::bit_common_remove_defarg_hash_functor))
        {
            std::size_t name_index = std::size_t(-1);

            const bool is_unordered_set_like = traits.IsUnorderedSetLike(name, &name_index);
            const bool is_unordered_map_like = traits.IsUnorderedMapLike(name, &name_index);

            if (
                is_unordered_set_like ||
                is_unordered_map_like
            )
            {
                std::size_t hash_targ_pos = std::size_t(-2); // `-2` is used to not enter the `if` below if none of the branches is taken and the assert is disabled.
                if (is_unordered_set_like)
                    hash_targ_pos = 1; // After the element type.
                else if (is_unordered_map_like)
                    hash_targ_pos = 2; // After the mapped type..
                else
                    assert(false && "This should be unreachable.");

                UnqualifiedName &name_part = name.parts.at(name_index);

                // The hash must be the last argument, otherwise removing it will mess up the order.
                if (name_part.template_args && name_part.template_args->args.size() == hash_targ_pos + 1)
                {
                    if (auto hash_type = std::get_if<Type>(&name_part.template_args->args.back().var))
                    {
                        if (
                            traits.IsHashFunctor(*hash_type) &&
                            hash_type->simple_type.name.parts.back().template_args &&
                            hash_type->simple_type.name.parts.back().template_args->args.size() == 1
                        )
                        {
                            if (auto hash_targ = std::get_if<Type>(&hash_type->simple_type.name.parts.back().template_args->args.front().var))
                            {
                                if (auto our_targ = std::get_if<Type>(&name_part.template_args->args.at(0).var))
                                {
                                    if (*our_targ == *hash_targ)
                                        name_part.template_args->args.pop_back();
                                }
                            }
                        }
                    }
                }
            }
        }

        // Rewrite template specializations as typedefs.
        if (bool(flags & SimplifyTypeNamesFlags::bit_common_rewrite_template_specializations_as_typedefs))
        {
            std::size_t name_index = std::size_t(-1);
            std::string_view new_name_base;
            bool allow_all_char_types = false;

            if (traits.SpecializationsHaveTypedefsForCharTypes(name, &name_index, &new_name_base, &allow_all_char_types))
            {
                UnqualifiedName &part = name.parts.at(name_index);

                // A separate condition to avoid going into the `else` below if this fails.
                if (part.template_args && part.template_args->args.size() == 1)
                {
                    if (auto type = std::get_if<Type>(&part.template_args->args.front().var))
                    {
                        const std::string_view type_word = type->AsSingleWord();
                        if (type_word == "char")
                        {
                            part.template_args.reset();
                            part.var = std::string(new_name_base);
                        }
                        else if (type_word == "wchar_t")
                        {
                            part.template_args.reset();
                            part.var.emplace<std::string>("w") += new_name_base;
                        }
                        else if (allow_all_char_types)
                        {
                            if (type_word == "char8_t")
                            {
                                part.template_args.reset();
                                part.var.emplace<std::string>("u8") += new_name_base;
                            }
                            else if (type_word == "char16_t")
                            {
                                part.template_args.reset();
                                part.var.emplace<std::string>("u16") += new_name_base;
                            }
                            else if (type_word == "char32_t")
                            {
                                part.template_args.reset();
                                part.var.emplace<std::string>("u32") += new_name_base;
                            }
                        }
                    }
                }
            }
        }
    }

    // Simplify cv-qualifiers according to the flags.
    // This is a low-level function, prefer `SimplifyTypeNames()`.
    CPPDECL_CONSTEXPR void SimplifyTypeCvQualifiers(SimplifyTypeNamesFlags flags, CvQualifiers &quals)
    {
        if (bool(flags & SimplifyTypeNamesFlags::bit_msvc_remove_ptr32_ptr64))
            quals &= ~(CvQualifiers::msvc_ptr32 | CvQualifiers::msvc_ptr64);
    }

    CPPDECL_CONSTEXPR void SimplifySimpleType(SimplifyTypeNamesFlags flags, SimpleType &simple_type)
    {
        if (bool(flags & SimplifyTypeNamesFlags::bit_common_remove_type_prefix))
            simple_type.prefix = SimpleTypePrefix{};
        if (bool(flags & SimplifyTypeNamesFlags::bit_common_remove_redundant_signed) && bool(simple_type.flags & SimpleTypeFlags::explicitly_signed) && !simple_type.IsNonRedundantlySigned())
            simple_type.flags &= ~SimpleTypeFlags::explicitly_signed;
    }

    // `target` is typically a `Type` or `Decl`.
    template <typename Traits = DefaultSimplifyTypeNamesTraits>
    CPPDECL_CONSTEXPR void SimplifyTypeNames(SimplifyTypeNamesFlags flags, auto &target, Traits &&traits = {})
    {
        if (bool(flags))
        {
            target.template VisitEachComponent<QualifiedName, CvQualifiers, SimpleType>(
                {},
                Overload{
                    [&](QualifiedName &name){SimplifyTypeQualifiedName(flags, name, traits);},
                    [&](CvQualifiers &quals){SimplifyTypeCvQualifiers(flags, quals);},
                    [&](SimpleType &quals){SimplifySimpleType(flags, quals);},
                }
            );
        }
    }
}

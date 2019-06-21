/**
 * @file    Serializable.h
 * @date    May 29 2019
 * @author  Quinn Mikelson
 *
 * @brief   Stores class metadata for serialization between objects and
 * serialized formats. References https://github.com/eliasdaler/MetaStuff
 */

#ifndef SERIALIZABLE_H
#define SERIALIZABLE_H

#include <functional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace Serializable
{
namespace detail
{
template <typename T, typename TupleType>
struct Holder;

// for_each_arg - call f for each argument from pack
template <typename F, typename... Args>
void for_each_arg(F&& f, Args&&... args);

// for_each_arg - call f for each element from tuple
template <typename F, typename TupleT>
void for_tuple(F&& f, TupleT&& tuple);

// overload for empty tuple which does nothing
template <typename F>
void for_tuple(F&& f, const std::tuple<>& tuple);

// calls F if condition is true
// this is useful for templated lambdas, because they won't be
// instantiated with unneeded types
template <bool Test, typename F, typename... Args, typename = std::enable_if_t<Test>>
void call_if(F&& f, Args&&... args);

// calls F if condition is false
template <bool Test, typename F, typename... Args,
          typename = std::enable_if_t<!Test>,
          typename = void>  // dummy type for difference between two functions
void call_if(F&& f, Args&&... args);
}  // namespace detail

template <typename... Args>
constexpr auto members(Args&&... args);

// function used for registration of classes by user
template <typename Class>
constexpr auto registerMembers();

// function used for registration of class name by user
template <typename Class>
auto registerName();

// returns set name for class
template <typename Class>
constexpr auto getName();

// Check if class has registerMembers<T> specialization (has been registered)
template <typename Class>
constexpr bool isRegistered();

// Check if Class has non-default ctor registered
template <typename Class>
constexpr bool ctorRegistered();

template <typename T>
struct constructor_args;

template <typename T>
using constructor_arguments = typename constructor_args<T>::types;

// Check if user registered non default constructor
template <typename Class>
constexpr bool ctorRegistered();

template <typename C, typename T>
class Member
{
public:
    // MemberTraits {
    using container_type = C;
    using value_type     = T;
    // 'pointer' is a member defined in container_type
    using pointer = T container_type::*;

    // @todo Should make getter/setter templates and use std::decay to allow const&,const*,etc...
    // template <typename MemberType>
    // using get_value_type = typename std::decay_t<MemberType>::value_type;
    using getter_function = std::function<T(C&)>;
    using setter_function = std::function<void(C&, T)>;
    // } MemberTraits

    // Member(const std::string label_in, pointer ptr_in) : label(label_in),
    // {
    // }
    Member(const std::string label_in, getter_function getter_in = {}, setter_function setter_in = {})
        : label(label_in), getter(getter_in), setter(setter_in)
    {
    }

    value_type get(container_type& container)
    {
        // if (member)
        //     return *member;
        return this->getter(container);
    }

    template <typename V, typename = std::enable_if_t<std::is_constructible<value_type, V>::value>>
    void set(container_type& container, V&& value)
    {
        // if (member)
        //     *member = value;
        this->setter(container, value);
    }

    const std::string getLabel() const
    {
        return this->label;
    }

private:
    std::string label{};
    Member::pointer member{nullptr};
    Member::getter_function getter{};
    Member::setter_function setter{};
};

template <class Derived>
class Container
{
public:
    Container()
    {
    }

    template <typename... T>
    Container(Member<Derived,T>&&... member_list)
    {
    }

    template<typename T>
    auto make_member(std::string label, std::function<T(Derived&)> getter, std::function<void(Derived&,T)> setter)
    {
        return Member<Derived,T>{label,getter,setter};
    }
    
    template <typename... T>
    auto members(Member<Derived,T>&&... member_list)
    {
        return std::make_tuple((member_list)...);
    }

    // // returns the number of registered members of the class
    // constexpr std::size_t getMemberCount()
    // {
    //     return std::tuple_size<decltype(getMembers())>::value;
    // }

    // returns std::tuple of Members
    auto getMembers(){
        return this->derived().registerMembers();
    }

    template<std::size_t I = 0, typename FuncT, typename... Tp>
    inline typename std::enable_if<I == sizeof...(Tp), void>::type
        for_each(std::tuple<Tp...> &, FuncT) // Unused arguments are given no names.
    { }

    template<std::size_t I = 0, typename FuncT, typename... Tp>
    inline typename std::enable_if<I < sizeof...(Tp), void>::type
        for_each(std::tuple<Tp...>& t, FuncT f)
    {
        f(std::get<I>(t));
        for_each<I + 1, FuncT, Tp...>(t, f);
    }
    
    template<typename T>
    T apply(std::string label){
        auto members = this->getMembers();
        void* retval;
        auto find_member = [&](auto mem){
            if(mem.getLabel() == label)
                mem.get(this->derived());
        };
        for_each(members,find_member);
        return *reinterpret_cast<T*>(retval);
    }

private:
    Derived& derived()
    {
        return static_cast<Derived&>(*this);
    }
    Derived const& derived() const
    {
        return static_cast<Derived const&>(*this);
    }
};

struct MyContainer : Container<MyContainer>
{
    auto registerMembers()
    {
        return members(make_member<int>("var", &MyContainer::getVar, &MyContainer::setVar),
                       make_member<std::string>("tar", &MyContainer::getTar, &MyContainer::setTar));
    }

    int var{1};
    std::string tar{"Default"};
    int getVar()
    {
        return var;
    }
    void setVar(int val)
    {
        var = val;
    }
    std::string getTar()
    {
        return tar;
    }
    void setTar(std::string val)
    {
        tar = val;
    }
    
};

MyContainer my_container;
int main()
{
    auto members = my_container.getMembers();
    auto print_info = [&](auto member){
        std::cout << member.getLabel() << '\n';
        std::cout << member.get(my_container) << '\n';
    };
    my_container.for_each(members,print_info);
    auto retval = my_container.get<int>("var");
    
    // mem.set(my_container,5);
    // std::cout << mem.get(my_container) << '\n';
    // std::cout << omem.getLabel() << '\n';
    // std::cout << omem.get(my_container) << '\n';
    // omem.set(my_container,"TAR");
    // std::cout << omem.get(my_container) << '\n';
    // std::cout << my_container.getMemberCount() << '\n';
    
}

}  // namespace Serializable

#include "Serializable.icc"

#endif

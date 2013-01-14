/**
 * This file implements the `JoinEvent` event.
 */

#define D2_SOURCE
#include <d2/events/join_event.hpp>

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_match.hpp>
#include <iostream>


namespace d2 {

extern std::ostream& operator<<(std::ostream& os, JoinEvent const& self) {
    os << self.parent << '^' << self.new_parent << '^' << self.child << '^';
    return os;
}

extern std::istream& operator>>(std::istream& is, JoinEvent& self) {
    using namespace boost::spirit::qi;

    unsigned long parent, new_parent, child;
    is >> match(ulong_ >> '^' >> ulong_ >> '^' >> ulong_ >> '^',
                parent, new_parent, child);
    self = JoinEvent();
    self.parent += parent;
    self.new_parent += new_parent;
    self.child += child;
    return is;
}

} // end namespace d2
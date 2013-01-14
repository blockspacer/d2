/**
 * This file implements the `ReleaseEvent` event.
 */

#define D2_SOURCE
#include <d2/events/release_event.hpp>

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_match.hpp>
#include <iostream>


namespace d2 {

extern std::ostream& operator<<(std::ostream& os, ReleaseEvent const& self) {
    os << self.thread << ';' << self.lock << ';';
    return os;
}

extern std::istream& operator>>(std::istream& is, ReleaseEvent& self) {
    using namespace boost::spirit::qi;

    is >> match(ulong_ >> ';' >> ulong_ >> ';', self.thread, self.lock);
    return is;
}

} // end namespace d2
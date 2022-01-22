#pragma once

// Forward declaring a class in a module seems to break things, with helpful error messages like "cannot convert MyClass* to MyClass*".
// Putting the forward declaration in a header fixes this issue. If proclaimed ownership declarations are implemented, the
// declaration below could be put into the module directly, and would look like something like the following:
//   extern module descriptor_pool : namespace vkw { class descriptor_pool; }
namespace vkw { class descriptor_pool; }

# Bee

## Building

Bee uses CMake as a cross-platform code build system and a high-level build manager called 'BeeBuild' - or `bb` for short. 
`bb` - among other things - abstracts away setting up and configuring CMake with all the right settings and handles that 
for you so that building the engine is a one-click process. `bb` will retrieve and build all dependencies, extract and
prepare all the tools binaries, bootstrap itself if running for the first time, and then build the engine code.

So, to build `Bee` open a command line and run: 

```batch
cd <Bee root>
./bb configure <generator>
```

Where `<generator>` is one of:
* VS2017
* VS2019
* CLion

Once `bb` finishes you can find your new project files in `<Bee root>/Build/<generator>`. Open the solution/project
(i.e. for visual studio it will be called `Bee.sln`) and build inside the IDE. 

NOTE: a `bb build` command is a TODO  that will probably take an hour to implement that I've been avoiding because 
it's less fun than doing graphics code but still important to do one day soon!

## Running the Bee.Sandbox demo app

Currently `Bee` is more of a loose collection of frameworks than a full-blown engine - there's no editor or anything 
at the moment and you have to write your own host application - but to see an example of the engine in flight you can
run the `Bee.Sandbox.Host` application/target (i.e. run the `Bee.Sandbox.Host` project from the visual studio solution). The 
sandbox app showcases the OS platform, input, Vulkan backend, and render graph modules and how to setup a host
program with a hot-reloadable client application.

## Architecture

### Plugin Architecture

`Bee` is based on a plugin-style architecture - every module in the engine is it's own hot-reloadable plugin dll. This 
was inspired by the work done by and written about the clever people at [The Machinery](https://ourmachinery.com) and 
initially I started it as a little experiment to try out those ideas but once I started writing c++ code like this it 
was too much fun to stop - it's like using a totally different and not-annoying programming language.

### `Bee.Core` - the standard library

Each plugin only links to one library internally (they are of course free to link with any dependencies they want) called 
`Bee.Core` which contains containers, memory allocators, logging, io etc. as well as the plugin registry and reflection 
API (more on this later). `Bee` uses `Bee.Core` instead of the C++ standard library for a few reasons:

* The standard headers are very bad for compile times due to extreme over-use of templates, bloated and 'catch-all' headers 
(such as `<utility>`) that make it hard to only include what you use, and just generally putting things headers that really 
should be put in cpp files. `Bee.Core` is very conservative about template usage outside of containers.
* The standard allocator API is complicated and encourages globals memory operations (making thread-safety hard). `Bee.Core` 
instead uses a far simpler per-instance model similar to the one discussed in [this paper](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2005/n1850.pdf).
* Strings are utf-8 on all platforms.
* `Bee` provides reflection and serialization of C++ types out of the box which is used across `Bee.Core` for i.e. containers
* API's can often take years to make it into the C++ standard and need to contend with decades of backward-compatibility 
considerations. We know `Bee` is used for `Bee`-based applications primarily in the interactive media space which means it's
 a lot easier to add functionality without having to wait for a new standard.
 
`Bee` plugins and programs should avoid using the standard library if possible however the engine does make efforts to provide adapters 
for operations such as memory allocation and hashing.

### `bee-reflect` and automatic serialization 

`Bee` has a full-blown, clang-based, template-supporting reflection system. It's pretty fast and non-intrusive compared to 
reflection libraries in many other C++ engines (such as the Unreal Header Tool) and runs as a CMake custom command whenever 
a reflected header file changes. It outputs .cpp files that implement reflection statically (found under `Build/Generated`) 
using code-generation although the actual output is still in flux. Users don't have to build the `bee-reflect` binary 
as it's distributed with the engine. Here's an example of using reflection in `Bee`:

```cpp
#include <Bee/Core/Reflection.hpp>

namespace my_namespace {

struct BEE_REFLECT(string_attribute = "Wow so cool") ReflectedStruct
{
    int                         int_val;
    float                       float_val;
    bee::DynamicArray<double>   array_of_doubles;
};

void do_something_cool_with_reflection()
{
    const type = bee::get_type<ReflectedStruct>();
    bee::log_info("Type: %s (\"%s\")", type->name, type->attributes[0].string);
    for (auto& field : type.as<bee::RecordType>().fields)
    {
        bee::log_info("\tField: %s (%s)", field.name, field.type->name);
    }
}

} // namespace my_namespace
```

Which should print out:

```
Type: my_namespace::ReflectedStruct ("Wow so cool")
    Field: int_val (int)
    Field: float_val (float)
    Field: array_of_doubles (bee::DynamicArray<double>)
```

`Bee` also has a robust serialization API that can perform 
both automatic and custom serialization (via the `SerializationBuilder` interface) of any reflected type including templates.
The core library provides three types of serializers - `JSONSerializer`, `BinarySerializer`, and `StreamSerializer` which is an adapter for the `bee::io::Stream` interface. 
Serializers can serialize/deserialize types using either a packed format with each field processed sequentially (faster) or a table format that supports robust forward and 
backward compatibility as well as removed and added fields (slower, more robust). `bee-reflect` supports versioning types and fields by default as well 
as hard serialization ordering constraints:

```
struct BEE_REFLECT(serializable, version = 2, format = packed) ReflectedStruct
{
    BEE_REFLECT(added = 1, order = 1)
    int                         int_val { 0 };
    BEE_REFLECT(added = 2, order = 3)
    float                       float_val { 1.0f };
    BEE_REFLECT(added = 1, removed = 2, order = 4)
    char                        char_val { 'x' };
    BEE_REFLECT(added = 1, order = 2)
    bee::DynamicArray<double>   array_of_doubles;
};
```

The above struct is in a packed format (which is also the default setting) and will process the fields in the order: 
`int_val`, `array_of_doubles`, `float_val`. However, if we're deserializing from file and the source binary/JSON is version=1 then 
`float_val` will be skipped and the default value of `1.0f` will remain. For more complex types you can use the `SerializationBuilder` 
interface with a special serialization function inside the `bee` namespace - here's an example straight from the engine source:

```cpp
template <typename T, ContainerMode Mode>
BEE_SERIALIZE_TYPE(SerializationBuilder* builder, Array<T, Mode>* array)
{
    int size = array->size();
    builder->container(SerializedContainerKind::sequential, &size);

    if (builder->mode() == SerializerMode::reading)
    {
        array->resize(size);
    }

    for (auto& element : *array)
    {
        builder->element(&element);
    }
}
```

**Note: templated types do not support automatic serialization.** You *must* implement a `BEE_SERIALIZE_TYPE` function 
for these types.

#### Building `bee-reflect`

A version of the `bee-reflect` binary is distributed with Bee. However, if you have to modify the source and build a new 
version of the tool you will need to do the following:

* First you will either need a version of cmake installed or have your environment variables point cmake invocations to the binary provided under
 `<bee root>/ThirdPart/Binaries/cmake/bin`
* Download the custom fork of LLVM located [here](https://github.com/jacobmilligan/llvm). **Note:** you *must* use this specific 
 fork as it contains some important modifications to i.e. support .inl files as header file extensions
* Build LLVM by invoking the `build.cmd` script located at the root of the newly downloaded LLVM fork. Now's the time to take a long bath or leisurely lunch break.
* Once LLVM has finished building create a new settings json file for `bb` (you can copy an existing one located in `CMake/Settings`) and add the following configuration options:
```json
{
    "cmake_options": {
        "BUILD_CLANG_TOOLS": "ON",
        "LLVM_INSTALL_DIR": "<Path to llvm fork root>/install/Release",
        "FORCE_ASSERTIONS_ENABLED": "ON",
        "DISABLE_REFLECTION": "ON"
    }
}
``` 
(adding `FORCE_ASSERTIONS_ENABLED` is optional but highly recommended)
* Re-configure the engine projects using `bb -c <generator> -s <path to settings json>`
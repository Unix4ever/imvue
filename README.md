# ImVue

Declarative reactive ImGui bindings inspired by Vue.js.
Almost no overhead, but relies on stl and c++11.

[![Build Status](https://travis-ci.org/Unix4ever/imvue.svg?branch=master)](https://travis-ci.org/Unix4ever/imvue)

Simple component template may look like that:

```
<template>
  <window name="test">
    <color-button desc-id="testing" size="20,20" col="0.6,0.9,0.6,1.0"/>
    <text-unformatted>Button was clicked {{self.counter}} times.</text-unformatted>
    <button v-on:click="self:testing()">click me</button>
  </window>
</template>

<script>
  return ImVue.new({
    testing = function(self)
      self.counter = self.counter + 1
    end,
    data = function(self)
      return {
        counter = 0
      }
    end
  })
</script>
```

Despite being similar to Vue.js, this project does not aim to be 100%
compatible with Vue templates.

It's possible to load components in the same way as it's implemented in Vue.js.
You can either define them using `lua` syntax or `imv` xml file.

`imv` files search path is configured using `package.imvpath` variable.

CSS Styles Support
------------------

ImVue supports CSS styling and HTML syntax to some extent.

[Styled document example](samples/simple/styled.xml) in action:

![demo](demo.gif)

Vue Special Syntax
------------------

### Supported

- `v-if/v-else-if/v-else`.
- `v-for` (does not support int index e.g.: `value, key, index`).
- `v-on`.
  - `v-on:(click|mousedown|mouseup|mouseover|mouseout)[.[ctrl|alt|meta|shift|exact]]`.
  - `v-on:(keyup|keydown|keypress)[.][<key_code>]`.
  - `v-on:change`.
- Attributes starting with `:` are treated as `v-bind:...`.
- Defining components and using custom tag names. Props validation is
  also supported.
- Component `<slot>` (limitations: no named slots, no default value,
  always using parent context).
- `ref` and `key` fields.

### Not Supported Yet

- Custom events: `v-on:yadda-yadda` & `this.$emit('yadda-yadda')`.
- `sync`.
- Defining events using `@` sign.
- Long definition using `v-bind`.
- Getting `event.target` in event handlers.
- Changing element properties using refs (RO access only).
- `${}` eval syntax in attributes.
- V8 JS integration.

### Lua Implementation Specifics

Each scripted attribute or evaluation is actual Lua code string, thus
there is no limitation on what you can do. Globals are also available.

Besides, it will create reactive listeners for each field that was used in the
evaluation.

Dependencies
------------

- ImGui without any modifications.
- NanoSVG is used for image rendering. Using customized rasterizer.
- RapidXML is used for XML parsing.
- customized version of [LibCSS](https://github.com/Unix4ever/libcss).

Optional Core Dependecies
-------------------------

- Lua 5.1+/LuaJIT 2.0.5+ - adds script interpreter.

Samples Dependencies
--------------------

- SDL2 >= 2.0.5
- libglew

Build
-----

### Simple Build

1. Install dependencies.
1. Run:

`make build`

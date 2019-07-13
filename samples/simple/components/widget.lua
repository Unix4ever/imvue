-- this is how you define the component using lua code
return ImVue.component('widget', {
  props = {
    count = {
      type = ImVue_Integer,
      required = true
    },
    title = {
      type = ImVue_String,
      default = "component"
    }
  },
  data = function(self)
    return {
      value = 'works'
    }
  end,
  template = '<window :name="self.title or \'component\'"><text-unformatted>local value: {{ self.value }}\nparent ctx counter: {{ self.count }}</text-unformatted><button v-on:click="self.value = self.count">update local value</button></window>'
})

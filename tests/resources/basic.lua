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
  template = '<window :name="self.title"><text-unformatted id="local">{{ self.value }}</text-unformatted><text-unformatted id="parent">{{ self.count }}</text-unformatted></window>'
})

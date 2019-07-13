
calls = {}
calls.increase = function(self, t)
  self[t] = (self[t] or 0) + 1
end

local callback = function(id)
  return function()
    calls:increase(id)
  end
end

return ImVue.component('lifecycle', {
  props = {
    'count'
  },
  data = function(self)
    return {
      data = function()
        return {}
      end,
      beforeCreate = callback('beforeCreate'),
      created = callback('created'),
      beforeMount = callback('beforeMount'),
      mounted = callback('mounted'),
      beforeUpdate = callback('beforeUpdate'),
      updated = callback('updated'),
      beforeDestroy = callback('beforeDestroy'),
      destroyed = callback('destroyed')
    }
  end,
  template = '<window :name="self.count"></window>'
})

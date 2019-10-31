local mixin = {
  data = function(self)
    return {
      test = 1
    }
  end
}

return ImVue.new({
  mixins = { mixin },
  testing = function(self)
    self.counter = self.counter + 1
    self.values[#self.values + 1] = 'element ' .. tostring(self.counter)
    self.progress = self.progress + 0.05
  end,
  runScript = function(self)
    local text = self._refs.input.buf
    if not text then
      return
    end

    local env = {}
    for k, v in pairs(_G) do
      env[k] = v
    end

    env.self = self

    local f
    if _VERSION == 'Lua 5.3' then
      f = load(text, 'script', 'bt', env)
    else
      f = loadstring(text, 'script')
      setfenv(f, env)
    end

    f()
  end,
  data = function(self)
    return {
      selected = -1,
      counter = 0,
      values = {},
      mode = nil,
      progress = 0,
      menu = {
        {
          label = "File",
          items = {
            {
              label = "Open",
              handler = function()
                self.mode = "open menu"
              end
            },
            {
              label = "Save",
              handler = function()
                self.mode = "save menu"
              end
            }
          }
        }
      }
    }
  end
})

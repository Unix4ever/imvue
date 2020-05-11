local asserts = require 'asserts'
result = {
  success = false
}

imvue = ImVue.new({
  runTest = function(self)
    asserts.equal(self._refs.container.size.x, 300)
    asserts.equal(self._refs.element1.size.x, 100)
    asserts.equal(self._refs.element2.size.x, 200)
    asserts.equal(self._refs.element3.size.x, 0)

    self.elements[3] = {
      style = [[
        flex-grow: 0;
        height: 50px;
      ]]
    }

    result.success = true
  end,
  data = function()
    return {
      elements = {
        {
          style = 'flex-grow: 1'
        },
        {
          style = 'flex-grow: 2'
        },
        {
          style = 'flex-grow: 0'
        }
      }
    }
  end
})

return imvue


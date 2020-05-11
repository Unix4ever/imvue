local asserts = {
    equal = function(arg1, arg2)
      if arg1 ~= arg2 then
        error("assertion failed: expected " .. tostring(arg1) .. " to be equal to " .. arg2)
      end
    end
}

return asserts

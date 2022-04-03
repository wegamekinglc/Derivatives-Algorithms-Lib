
function Format(format :: Ptr{Uint8}, arg1 :: Array{CELL, 2}, arg2 :: Array{CELL, 2}, arg3 :: Array{CELL, 2}, arg4 :: Array{CELL, 2}, arg5 :: Array{CELL, 2}, arg6 :: Array{CELL, 2}, arg7 :: Array{CELL, 2}, arg8 :: Array{CELL, 2}, arg9 :: Array{CELL, 2})
    biscuit = ccall((:ffi_Format, DLLPath), Float64, (Ptr{Uint8}, Int32, Int32, Int32, Int32, Int32, Int32, Int32, Int32, Int32), ToCookie(format), ToCookie(arg1), ToCookie(arg2), ToCookie(arg3), ToCookie(arg4), ToCookie(arg5), ToCookie(arg6), ToCookie(arg7), ToCookie(arg8), ToCookie(arg9))
    CheckBiscuit(biscuit, 0+1)
	result = OutMatrixCell(biscuit, convert(Int32, 0))
    return (result,)
end

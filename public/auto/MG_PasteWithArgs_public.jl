
function PasteWithArgs(func_name :: Ptr{Uint8})
    biscuit = ccall((:ffi_PasteWithArgs, DLLPath), Float64, (Ptr{Uint8}), ToCookie(func_name))
    CheckBiscuit(biscuit, 0+1)
	func_with_args = OutString(biscuit, convert(Int32, 0))
    return (func_with_args,)
end

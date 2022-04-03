
function Repository_Size()
    biscuit = ccall((:ffi_Repository_Size, DLLPath), Float64, ())
    CheckBiscuit(biscuit, 0+1)
	size = OutInt(biscuit, convert(Int32, 0))
    return (size,)
end


function Repository_Find(match :: Ptr{Uint8})
    biscuit = ccall((:ffi_Repository_Find, DLLPath), Float64, (Ptr{Uint8}), ToCookie(match))
    CheckBiscuit(biscuit, 0+1)
	objects = OutVectorHandle(biscuit, convert(Int32, 0))
    return (objects,)
end

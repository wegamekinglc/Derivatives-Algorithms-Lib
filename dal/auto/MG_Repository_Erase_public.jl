
function Repository_Erase(objects :: Array{Handle, 1})
    biscuit = ccall((:ffi_Repository_Erase, DLLPath), Float64, (Int32), ToCookie(objects))
    CheckBiscuit(biscuit, 0+1)
	num_erased = OutInt(biscuit, convert(Int32, 0))
    return (num_erased,)
end

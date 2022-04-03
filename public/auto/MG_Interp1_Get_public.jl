
function Interp1_Get(f :: Handle, x :: Array{Float64, 1})
    biscuit = ccall((:ffi_Interp1_Get, DLLPath), Float64, (Int32, Int32), ToCookie(f), ToCookie(x))
    CheckBiscuit(biscuit, 0+1)
	y = OutVectorDouble(biscuit, convert(Int32, 0))
    return (y,)
end

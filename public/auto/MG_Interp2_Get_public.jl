
function Interp2_Get(f :: Handle, x :: Array{Float64, 1}, y :: Array{Float64, 1})
    biscuit = ccall((:ffi_Interp2_Get, DLLPath), Float64, (Int32, Int32, Int32), ToCookie(f), ToCookie(x), ToCookie(y))
    CheckBiscuit(biscuit, 0+1)
	z = OutMatrixDouble(biscuit, convert(Int32, 0))
    return (z,)
end

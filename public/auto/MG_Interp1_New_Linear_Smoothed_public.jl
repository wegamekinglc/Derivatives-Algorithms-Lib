
function Interp1_New_Linear_Smoothed(name :: Ptr{Uint8}, x :: Array{Float64, 1}, y :: Array{Float64, 1}, smoothing :: Float64, fit_weights :: Array{Float64, 1})
    biscuit = ccall((:ffi_Interp1_New_Linear_Smoothed, DLLPath), Float64, (Ptr{Uint8}, Int32, Int32, Float64, Int32), ToCookie(name), ToCookie(x), ToCookie(y), ToCookie(smoothing), ToCookie(fit_weights))
    CheckBiscuit(biscuit, 0+1)
	f = OutHandle(biscuit, convert(Int32, 0))
    return (f,)
end

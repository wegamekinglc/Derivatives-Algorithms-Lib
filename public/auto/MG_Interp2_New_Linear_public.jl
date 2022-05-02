
function Interp2_New_Linear(name :: Ptr{Uint8}, x :: Array{Float64, 1}, y :: Array{Float64, 1}, z :: Array{Float64, 2})
    biscuit = ccall((:ffi_Interp2_New_Linear, DLLPath), Float64, (Ptr{Uint8}, Int32, Int32, Int32), ToCookie(name), ToCookie(x), ToCookie(y), ToCookie(z))
    CheckBiscuit(biscuit, 0+1)
	f = OutHandle(biscuit, convert(Int32, 0))
    return (f,)
end

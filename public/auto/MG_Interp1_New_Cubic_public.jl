
function Interp1_New_Cubic(name :: Ptr{Uint8}, x :: Array{Float64, 1}, y :: Array{Float64, 1}, boundary_order :: Array{Int32, 1}, boundary_value :: Array{Float64, 1})
    biscuit = ccall((:ffi_Interp1_New_Cubic, DLLPath), Float64, (Ptr{Uint8}, Int32, Int32, Int32, Int32), ToCookie(name), ToCookie(x), ToCookie(y), ToCookie(boundary_order), ToCookie(boundary_value))
    CheckBiscuit(biscuit, 0+1)
	f = OutHandle(biscuit, convert(Int32, 0))
    return (f,)
end

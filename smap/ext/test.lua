
function test(t)
	for i,v in pairs(t) do
	   if type(v) == "table" then
	    test(v);
	   else
	    print(i.." = "..tostring(v));
	   end  
	end
	t = {a = "haha", b = "hoho", "shit"}
	print(ptable(t));
    return 0.1, "GOOOOOOOOOD";
end;
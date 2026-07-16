
//:IMPORT std/stdObject.js
//:EXPORT stdString


function stdString()
{
    stdObject.apply(this);
}


stdString.i_divNumber = function(num)
{
    var len = num.length;
    if ( len <= 3 )
	return num;
    return stdString.i_divNumber(num.substr(0,len-3))+","
	+num.substr(len-3);
	
}

stdString.divNumber = function(num)
{
    var num1 = num+"";
    return stdString.i_divNumber(num1);
}


stdString.startAndLast = function(str,n,m)
{
    if ( !str )
	return "";
    if ( str.length < n+m+1 )
	return str;
    return str.substr(0,n)+" ... "+str.substr(str.length - m);
}

stdObject.defClass
(
    "stdString",
    stdString,
    stdObject
);



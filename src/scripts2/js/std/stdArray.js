
//:IMPORT std/stdObject.js
//:EXPORT stdArray


function stdArray()
{
    stdObject.apply(this);
}


stdArray.swap = function(ary,n1,n2) {
    var ret;
    if ( n1 == null )
	return ary.concat();
    if ( n2 == null ) {
	var ret = ary.concat();
	ret.splice(n1,1);
	return ret;
    }
    else {
	var ret = ary.concat();
	var tmp = ret[n1];
	ret[n1] = ret[n2];
	ret[n2] = tmp;
    }
    return ret;
}


stdObject.defClass
(
    "stdArray",
    stdArray,
    stdObject
);



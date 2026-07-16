
//:IMPORT std/stdObject.js
//:EXPORT stdJSON


function stdJSON()
{
    stdObject.apply(this);
}

stdJSON.strfill = function(inp,n) {
    var r = inp.toString().split('');
    while(r.length <= n) {
	r.unshift(' ');
    }
    return r.join('');
}


stdJSON.arySwap = function(ary,n1,n2) {
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

stdJSON.parse = function(str) {
    if ( str == null )
	return null;
    var ret = stdJSON.stripComments(str);
    return JSON.parse(ret);
}

stdJSON.stripComments = function (str) {
    if ( str.indexOf("//") < 0 && str.indexOf("/*") < 0 )
	return str;
    var rg2 = new RegExp(/(\"([^\"]*\"))/g);
    var ret = "";
    for ( ; ; ) {
	var rg = new RegExp(/(\"([^\"]*\\\")*([^\"]*\"))|(\'([^\']*\\\')*([^\']*\'))|(\/\*(.|\n|\r)*\*\/)|(\/\/[^\n\r]*)/g);
	var ary = rg.exec(str);
	if ( !ary )
	    return ret + str;
	if ( str.indexOf('"',ary.index) == ary.index ||
	     str.indexOf("'",ary.index) == ary.index ) {
	    ret += str.substr(0,rg.lastIndex);
	    str = str.substr(rg.lastIndex);
	    continue;
	}
	if ( str.indexOf("//",ary.index) == ary.index ) {
	    ret += str.substr(0,ary.index);
	    str = str.substr(rg.lastIndex);
	    continue;
	}
	ret += str.substr(0,ary.index)
	    + str.substr(ary.index,rg.lastIndex - ary.index)
	    .replace(/[^\n\r]+/mg,"");
	str = str.substr(rg.lastIndex);
    }
}


stdObject.defClass
(
    "stdJSON",
    stdJSON,
    stdObject
);



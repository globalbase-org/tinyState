//:EXPORT stdObject



function stdObject() {
    this.i_getSeq();
}


stdObject.defClass = function(className,subclass,superclass,instance) {

    if ( superclass )
	subclass.prototype = new superclass();
    if ( !instance )
	return;
    for ( ix in instance ) {
	subclass.prototype[ix] = instance[ix];
    }
    subclass.prototype.className = className;
    subclass.prototype.i_classNameList = [className];
    if ( superclass ) {
	subclass.prototype.i_classNameList = 
	    subclass.prototype.i_classNameList.concat(superclass.prototype.i_classNameList);
    }
    stdObject.classTable[className] = subclass;
}

stdObject.i_seqNo = 1;
stdObject.classTable = {};
stdObject.isClassPrototype = function(pclass,className) {
    if ( !pclass )
	return false;
    for ( ix in pclass.prototype.i_classNameList ) {
	if ( pclass.prototype.i_classNameList[ix] == className )
	    return true;
    }
    return false;
}

stdObject.copyHash = function(hash) {
    if ( Array.isArray(hash) ) {
	var ret = [];
	for ( ix in hash ) {
	    ret[ix] = hash[ix];
	}
	return ret;
    }
    else {
	var ret = {};
	for ( ix in hash ) {
	    ret[ix] = hash[ix];
	}
	return ret;
    }
}

stdObject.defClass
(
    "stdObject",
    stdObject,
    null,
    {
	i_getSeq : function() {
	    stdObject.i_seqNo ++;
	    if ( stdObject.i_seqNo <= 0 )
		stdObject.i_seqNo = 1;
	    this.i_seq = stdObject.i_seqNo;
	    return this.i_seq;
	},
	dump : function(msg) {
	    console.log(msg);
	},
	seq : function() {
	    return this.i_seq;
	},
	classNameList : function() {
	    var i;
	    var ret = "";
	    for ( i = 0 ; i < this.i_classNameList.length ; i ++ ) {
		if ( i != 0 )
		    ret += ":";
		ret += this.i_classNameList[i];
	    }
	    return ret;
	},
	isClass : function(className) {
	    for ( ix in this.i_classNameList ) {
		if ( this.i_classNameList[ix] == className )
		    return true;
	    }
	    return false;
	},

	i_seq : 0,


    }
);






//:IMPORT gt/gtScope.js
//:EXPORT gtTable

function gtTable(gtScp,parameters) {
    if ( arguments.length == 0 )
	return;

    this.i_headerLength = 1;
    this.i_footerLength = 0;
    this.i_merginLength = 1;
    this.i_format = null;
    this.i_gtTable_array_flag = true;
    this.i_key = "none";
    this.i_nodeID = 1;
    this.i_activeTable = [];
/* activeTable
   {
   "uiObject" : 
   "remove" : 
   }
*/


    gtScope.apply(this,[gtScp,null,false,parameters]);

    this.initial("gtTable");
}


gtTable.symArray = "v.gtTable_array";
gtTable.symTarget = "v.gtTable_target";
gtTable.symLength = "v.gtTable_length";
gtTable.symIndex = "v.gtTable_index";
gtTable.symSelected = "v.gtTable_selected";
gtTable.symSelectedKey = "v.gtTable_selectedKey";
gtTable.symSelectedIndex = "v.gtTable_selectedIndex";
gtTable.symHeaderLength= "v.gtTable_HeaderLength";
gtTable.symFooterLength= "v.gtTable_FooterLength";


stdObject.defClass
(
    "gtTable",
    gtTable,
    gtScope,
    {
	i_selectedAction : true,
	i_eChangeSetting : false,

	i_selectedActionHandler : function() {
	    if ( !this.i_selectedAction )
		return;
	    var sel = this.uiObject.selectedIndex;
	    if ( typeof(sel) != "number" ) {
		this.i_selectedAction = false;
		return;
	    }
	    if ( sel < 0 ) {
		this.i_selectedAction = true;
		this.piggybackTurtle.bind(this,gtTable.symSelectedIndex,sel);
		this.piggybackTurtle.bind(this,gtTable.symSelected,"");
		this.piggybackTurtle.bind(this,gtTable.symSelectedKey,"");
		if ( !this.i_eChangeSetting ) {
		    this.piggybackTurtle.bind(this,"e.change");
		    this.piggybackTurtle.refer(this,"e.change",this,this.i_selectedActionHandler);
		    this.i_eChangeSetting = true;
		}
		return;
	    }
	    var opt = this.uiObject.options[sel];
	    if ( !opt ) {
		this.i_selectedAction = false;
		return;
	    }
	    this.i_selectedAction = true;
	    if ( !this.i_eChangeSetting ) {
		this.piggybackTurtle.bind(this,"e.change");
		this.piggybackTurtle.refer(this,"e.change",this,this.i_selectedActionHandler);
		this.i_eChangeSetting = true;
	    }
	    this.piggybackTurtle.bind(this,gtTable.symSelectedIndex,sel);
	    if ( !opt.gtPane ) {
		this.piggybackTurtle.bind(this,gtTable.symSelected,"");
		this.piggybackTurtle.bind(this,gtTable.symSelectedKey,"");
	    }
	    else {
		var dt = opt.gtPane.piggybackTurtle.refer(
		    this,gtTable.symTarget);
		if ( dt.getType() == "null" )
		    dt = "";
		this.piggybackTurtle.bind(
		    this,gtTable.symSelected,dt);
		this.piggybackTurtle.bind(
		    this,gtTable.symSelectedKey,
		    this.i_getKey(dt.d));
	    }
	},

	i_gtTable_array_handler : function(ev) {
	    var ff = this.i_gtTable_array_flag;
	    this.i_gtTable_array_flag = true;
	    if ( !ff )
		this.wakeup();
	},

	i_getKey : function(ele) {
	    var ret;
	    if ( this.i_key == "this" )
		ret = ele;
	    else if ( this.i_key == "none" )
		return "";
	    else {
		var k = this.i_key;
		var ee = ele;
		for ( ; ; ) {
		    var ix = k.indexOf(".");
		    if ( ix < 0 )
			break;
		    if ( ix == 0 ) {
			k = k.substr(1);
			continue;
		    }
		    ee = ee[k.substr(0,ix)];
		    k = k.substr(ix);
		}
		ret = ee[k];
	    }
	    if ( typeof ret == "string" )
		return ret;
	    if ( typeof ret == "number" )
		return ret;
	    this.dump("ERROR("+this.gid+") :: gtTable element type missmatch ("+ret+")");
	    return ret;
	},

	i_dump_tbl : function(msg,tbl) {
	    var i;
	    if ( !Array.isArray(tbl) ) {
		this.dump("TABLE "+msg+" NOT ARRAY");
		return;
	    }
	    this.dump("TABLE "+msg);
	    for ( i = 0 ; i < tbl.length ; i ++ )
		this.dump("   "+i+" : "+tbl[i].remove+" => "+tbl[i].uiObject);
	},

	/*=====================================================
	 *
	 *    tinyState STATE MACHINE
	 *
	 *=====================================================*/

	INI_gtObject_PLUGIN : function(ev) {
	    this.piggybackTurtle.bind(this,gtTable.symArray,[]);
	    this.piggybackTurtle.bind(this,gtTable.symLength,0);
	    this.piggybackTurtle.refer(
		this,
		gtTable.symArray,
		this,
		this.i_gtTable_array_handler);
	    return "doINI_gtObject_LAUNCH";
	},

	INI_gtScope_END : function(ev) {
	    var gf = this.uiObject.getAttribute("gformat");
	    if ( !gf )
		return "doFIN_START";
	    this.i_uiList = this.uiObject;
	    this.i_insertPoint = "none";
	    this.i_null = "get";
	    var gfary = gf.split(/[ \t]*;[ \t]*/);
	    for ( var ix = 0 ; ix < gfary.length ; ix ++ ) {
		var cmd = gfary[ix];
		var sep = cmd.split(/[ \t]*:[ \t]*/);
		if ( !sep[0] || !sep[1] )
		    continue;
		switch ( sep[0] ) {
		case "gid":
		    var pgid = this.uiObject.getAttribute("gid") ||
			this.uiObject.getAttribute("id");
		    this.i_format = this.getFormatDOM(sep[1],pgid);
		    break;
		case "header":
		    this.i_insertPoint = sep;
		    break;
		case "footer":
		    this.i_insertPoint = sep;
		    break;
		case "key":
		    this.i_key = sep[1];
		    break;
		case "null":
		    this.i_null = sep[1];
		    break;
		default:
		    this.dump("undefined field [gformat] = "+sep[0]);
		}
	    }
	    if ( !this.i_format )
		return "doFIN_START";
	    if ( this.i_insertPoint == "none" )
		return "doINI_gtTable_INSERT_POINT";
	    var gt = this.getGtObjectById(this.i_insertPoint[1]);
	    if ( gt == null ) {
		this.i_insertPoint = "none";
		return "doINI_gtTable_INSERT_POINT";
	    }
	    if ( !gt.uiObject.parentNode ) {
		this.i_insertPoint = "none";
		return "doINI_gtTable_INSERT_POINT";
	    }
	    this.i_uiList = gt.uiObject.parentNode;
	    this.i_marginLength = this.i_uiList.childNodes.length;
	    if ( this.i_insertPoint[0] == "footer" ) {
		this.i_lastPoint = gt.uiObject;
		var i;
		for ( i = 0 ; i < this.i_uiList.childNodes.length ; i ++ )
		    if ( this.i_uiList.childNodes[i] == gt.uiObject )
			break;
		this.i_headerLength = i;
		this.i_footerLength = this.i_marginLength - this.i_headerLength;
	    }
	    else {
		var i,j;
		for ( i = 0 ; i < this.i_uiList.childNodes.length ; i ++ )
		    if ( this.i_uiList.childNodes[i] == gt.uiObject )
			break;
		for ( j = i+1 ; j < this.i_uiList.childNodes.length ; j ++ )
		    if ( this.i_uiList.childNodes[j] )
			break;
		if ( j == this.i_uiList.childNodes.length )
		    this.i_lastPoint = null;
		else
		    this.i_lastPoint = this.i_uiList.childNodes[j];
		this.i_headerLength = i+1;
		this.i_footerLength = this.i_marginLength - this.i_headerLength;
	    }
	    return "doINI_gtTable_INSERT_POINT";
	},
	INI_gtTable_INSERT_POINT : function(ev) {
	    if ( this.i_insertPoint == "none" ) {
		this.i_marginLength = this.uiObject.childNodes.length;
		this.i_footerLength = 0;
		this.i_headerLength = this.i_marginLength;
		this.i_lastPoint = null;
	    }
	    this.piggybackTurtle.bind(this,gtTable.symHeaderLength,this.i_headerLength);
	    this.piggybackTurtle.bind(this,gtTable.symFooterLength,this.i_FooterLength);
	    this.i_selectedActionHandler();
	    return "doACT_START";
	},

	ACT_TINYSTATE_START : function(ev) {
	    return "doACT_KEY_1";
	},
	ACT_KEY_1 : function(ev) {
	    if ( !this.i_gtTable_array_flag )
		return "doACT_KEY_FINISH";
	    this.i_gtTable_array_flag = false;
	    this.i_tbl = this.piggybackTurtle.refer(this,gtTable.symArray);
	    if ( this.i_tbl.getType() != "array" )
		return "doACT_KEY_FINISH";
	    if ( this.i_null == "get" ) {
		this.i_tbl = this.i_tbl.d.concat();
		for ( i = 0 ; i < this.i_tbl.length ; i ++ )
		    if ( this.i_tbl[i] === null )
			this.i_tbl[i] = "null";
	    }
	    else {
		var tbl = [];
		for ( i = 0 ; i < this.i_tbl.d.length ; i ++ ) {
		    if ( this.i_tbl.d[i] !== null )
			tbl.push(this.i_tbl.d[i]);
		}
		this.i_tbl = tbl;
	    }
	    this.piggybackTurtle.bind(this,gtTable.symLength,this.i_tbl.length);
	    
	    if ( this.i_key != "none" )
		return "doACT_KEY_2";

	    var i = 0;
	    for ( ; i < this.i_tbl.length && i < this.i_activeTable.length ; i ++ ) {
		this.i_activeTable[i].uiObject.gtPane.piggybackTurtle.bind(
		    this,
		    gtTable.symTarget,
		    this.i_tbl[i]);
		this.i_activeTable[i].uiObject.gtPane.piggybackTurtle.bind(
		    this,
		    gtTable.symIndex,
		    i);
	    }
	    if ( i < this.i_tbl.length ) {
		for ( ; i < this.i_tbl.length ; i ++ ) {
		    var nd = this.appendDOMformat(
			this.i_uiList,
			this.i_lastPoint,
			this.i_format,
			{
			    "gid" : "__tblNode"+i,
			    "piggybackTurtle" : [
				{"name" : gtTable.symTarget,
				 "value" : this.i_tbl[i]}
			    ]
			});
		    this.i_activeTable[i] = {
			"uiObject" : nd,
			"remove" : false
		    };
		}
		return "doACT_KEY_FINISH";
	    }
	    for ( ; i < this.i_activeTable.length ; i ++ )
		this.removeDOM(this.i_activeTable[i].uiObject);
	    this.i_activeTable.length = this.i_tbl.length;
	    return "doACT_KEY_FINISH";
	},
	ACT_KEY_2 : function(ev) {
	    var new_keys = {};
	    for ( var i = 0 ; i < this.i_tbl.length ; i ++ ) {
		var ky = this.i_getKey(this.i_tbl[i]);
		new_keys[ky] = {"data" : this.i_tbl[i]};
	    }
	    var old_keys = {};
	    for ( var i = 0 ; i < this.i_activeTable.length ; i ++ ) {
		var at = this.i_activeTable[i];
		var ky = this.i_getKey(at.uiObject.gtPane.gtTable_target);
		old_keys[ky] = {"num" : i};
		if ( new_keys[ky] )
		    continue;
		at.remove = true;
		this.removeDOM(at.uiObject);
	    }
	    var newActiveTable = [];
	    var i = 0;
	    var j = 0;
	    for (  ; i < this.i_tbl.length &&
		  j < this.i_activeTable.length ; ) {
		var nkey = this.i_getKey(this.i_tbl[i]);
		var ele = this.i_activeTable[j];
		if ( ele.remove ) {
		    j ++;
		    continue;
		}
		var okey = this.i_getKey(ele.uiObject.gtPane.gtTable_target);
		if ( nkey == okey ) {
		    ele.uiObject.gtPane.gtTable_target = this.i_tbl[i];
		    ele.uiObject.gtPane.piggybackTurtle.bind(
			this,
			gtTable.symTarget,
			this.i_tbl[i]);
		    newActiveTable[i] = ele;
		    j ++;
		    i ++;
		    continue;
		}
		var optr = old_keys[nkey];
		if ( optr ) {
		    newActiveTable[i] = this.i_activeTable[optr.num];
		    this.i_activeTable[optr.num] = {"remove" : true};
		    this.i_uiList.insertBefore(
			newActiveTable[i].uiObject,
			this.i_activeTable[j].uiObject);
		    var ele1 = newActiveTable[i];
		    ele1.uiObject.gtPane.gtTable_target = this.i_tbl[i];
		    ele1.uiObject.gtPane.piggybackTurtle.bind(
			this,
			gtTable.symTarget,
			this.i_tbl[i]);
		    i ++;
		    continue;
		}
		var nd = this.appendDOMformat(
		    this.i_uiList,
		    this.i_activeTable[j].uiObject,
		    this.i_format,
		    {
			"gid" : "__tblNode"+this.i_nodeID,
			"piggybackTurtle" : [
			    {"name" : gtTable.symTarget,
			     "value" : this.i_tbl[i]}
			]
		    });
		nd.gtPane.gtTable_target = this.i_tbl[i];
		this.i_nodeID ++;
		newActiveTable[i] = {
		    "uiObject" : nd,
		    "remove" : false};
		i ++;
	    }
	    for ( ; i < this.i_tbl.length ; i ++ ) {
		var nd = this.appendDOMformat(
		    this.i_uiList,
		    this.i_lastPoint,
		    this.i_format,
		    {
			"gid" : "__tblNode"+this.i_nodeID,
			"piggybackTurtle" : [
			    {"name" : gtTable.symTarget,
			     "value" : this.i_tbl[i]}
			]
		    });
		nd.gtPane.gtTable_target = this.i_tbl[i];
		this.i_nodeID ++;
		newActiveTable[i] = {
		    "uiObject" : nd,
		    "remove" : false};
	    }
	    this.i_activeTable = newActiveTable;
	    for ( i = 0 ; i < this.i_activeTable.length ; i ++ ) {
		this.i_activeTable[i].uiObject.gtPane.piggybackTurtle.bind(
		    this,
		    gtTable.symIndex,
		    i);
	    }
	    return "doACT_KEY_FINISH";
	},
	ACT_KEY_FINISH : function(ev) {
	    this.i_selectedAction = true;
	    this.i_selectedActionHandler();
	    return "ACT_START";
	},

	FIN_START : function(ev) {
	    this.i_activeTable = null;
	    this.i_lastPoint = null;
	    this.i_uiList = null;
	    return "doFIN_gtScope_START";
	},
    }
);

function Homeauto() {
	

	this.create_item = function(name, widget) {
		
		var namespan = $("<span></span>")
			.html(name);
		
		var leftlabel = $("<div></div>")
			.addClass("leftlabel")
			.append(namespan);
		
		var rightwidget = $("<div></div>")
			.addClass("rightwidget")
			.append(widget);
		
		var item = $("<div></div>")
			.addClass("item")
			.append(leftlabel)
			.append(rightwidget);
		
		return item;
	}
	
	
	this.create_switch = function(id, context, name, initval) {
	
		if ( initval ) {
			initval = true;
		} else {
			initval = false;
		}

		var switchinput = $("<input></input>")
			.attr("id", id)
			.attr("type", "checkbox")
			.attr("name", "onoffswitch")
			.prop("checked", initval)
			.addClass("onoffswitch-checkbox")
			.attr("onchange", "homeauto.switch_handler("+id+", this.checked)");
		
		var labelinner = $("<span></span>")
			.addClass("onoffswitch-inner");
		
		var labelswitch = $("<span></span>")
			.addClass("onoffswitch-switch");
		
		var switchlabel = $("<label></label>")
			.addClass("onoffswitch-label")
			.attr("for", id)
			.append(labelinner)
			.append(labelswitch);
		
		
		var switchdiv = $("<div></div>")
			.addClass("onoffswitch")
			.append(switchinput)
			.append(switchlabel);
		
		var item = this.create_item(name, switchdiv);
		
		return item;
	}
	
	
	
	this.create_slider = function(id, context, name, min, max, initval) {


		var sliderlabel = $("<div></div>")
			.attr("id", id+"label")	
			.html(initval);		
			
		var sliderinput = $("<input></input>")
			.attr("id", id)
			.attr("type", "range")
			.attr("min", min)
			.attr("max", max)
			.attr("value", initval)
			.attr("oninput", "homeauto.slider_handler("+id+", this.value)")
			.attr("onchange", "homeauto.slider_handler("+id+", this.value)");
			
				
		
		var sliderdiv = $("<div></div>")
			.addClass("rangeslider")
			.append(sliderinput)
			//.append(sliderlabel);
		
		var item = this.create_item(name, sliderdiv);
		
		return item;
	}
	
	
	this.create_button = function(id, context, name, label, initval) {

		var buttoninput = $("<input></input>")
			.addClass("button")
			.attr("id", id)
			.attr("type", "button")
			.attr("value", label)
			.attr("onclick", "homeauto.button_handler("+id+")");
		
		var buttondiv = $("<div></div>")
			.addClass("button")
			.append(buttoninput);
		
		var item = this.create_item(name, buttondiv);
		
		return item;
	}
				
				
				
				
				
	this.switch_handler = function(id, checked) {
		$.get("/set?" +  id + "=" + checked, function(data) { });
	}
	
	this.slider_timeout = null;
	this.slider_value = 0;
	this.slider_handler = function(id, value) {
		/*
		slider = $("#"+id);
		
		
		width = slider.width();
		console.log(width);
		
		span = parseInt(slider.attr("max")) - parseInt(slider.attr("min"));
		
		console.log(span);
		
		
		pos = slider.offset()
		pos.left = 10 + parseInt(pos.left) + parseInt(slider.val()) * width * 0.75 / span;
		
		
		console.log(pos);
		
		
		$("#"+id+"label")
			.html(value)			
			.offset(pos);
			*/
		this.slider_value = value;
		if ( this.slider_timeout == null ) {
			this.slider_timeout = setTimeout(function() { 
				$.get("/set?" +  id + "=" + homeauto.slider_value, function(data) { });
				//clearTimeout(homeauto.slider_timeout);
				homeauto.slider_timeout = null;
			}, 200);
		}
	}
	
	this.button_handler = function(id) {
		$.get("/set?" +  id + "=true", function(data) { });
	}
				
	
	// Callback on load finished
	this.load_success = function(data, textStatus, xhr) { 
		this.update(xhr.responseText);
	};
	
	this.ajax_error = function(jqXHR, textStatus, errorThrown) {
		homeauto.check_offline("offline");
		console.log("AJAX error: " + textStatus + " :: " + errorThrown + " :: " + jqXHR.responseText);
	}



	this.update_switch = function(id, value) {
		if ( value ) {
			value = true;
		} else {
			value = false;
		}
		console.log("switch #" + id + " = " + value);  
		$("#"+id).prop("checked", value);
	}
	
	this.update_slider = function(id, value) {
		console.log("slider #" + id + " = " + value);  
		$("#"+id).val(value);			
	}
	
	this.update_button = function(id, label) {		
		console.log("switch #" + id);  
	}

	
	this.set_normal_update = function() {
		clearInterval(this.update_timeout);		
		this.update_timeout = setInterval( this.update_start, 2000);
	}
	
	this.set_slow_update = function() {
		clearInterval(this.update_timeout);		
		this.update_timeout = setInterval( this.update_start, 10000);
	}
	
	
	this.show = function() {
		this.update_start();
		if ( this.offline ) {
			this.set_slow_update();
		} else {
			this.set_normal_update();
		}
	}
	
	this.hide = function() {
		clearInterval(this.update_timeout);
	}
	
	this.update_timeout = null;	
	this.update_count = 0;
	
	this.offline = false;
	
	this.check_offline = function(data) {				
		// Server offline, serving offline.html from cache
		if ( data == "offline" ) {
			if ( this.offline == false ) {
				$("#main").hide();
				$("#offline").show();				
				this.set_slow_update();
				//document.write("Server offline...");				
			}
			this.offline = true;
			return true;
		}
		
		// No longer offline, reload
		if ( this.offline ) {
			this.offline = false;
			$("#offline").hide();
			$("#main").show();
			this.set_normal_update();
			this.load_start();
		}
		return false;
	}
	
	this.update_handler = function(data) {
		
		console.log(">" + data);
		
		data = JSON.parse(data);
		
		
		if ( this.check_offline(data) ) {
			return;
		}
		
		
		for ( var i in data ) {
			console.log(data[i][0]);
			if ( data[i][0] == "switch" ) {				
				this.update_switch(data[i][1], data[i][2]);
			} else if ( data[i][0] == "slider" ) {				
				this.update_slider(data[i][1], data[i][2]);
			} else if ( data[i][0] == "button" ) {				
				this.update_button(data[i][1], data[i][2]);
			}	
		}		
	}
	
	
		
	this.update_start = function() {
		++homeauto.update_count;
		$.get("/update?"+homeauto.update_count, function(data) { 
			homeauto.update_handler(data); 
		});		
	}	
	
	

	
	this.load_handler = function(data) {		
		//alert(data);
		console.log(data);
		
		data = JSON.parse(data);
		
		var main = $("#main");
		main.html("");
		
		if ( this.check_offline(data) ) {
			return;
		}
		
		for ( var i in data ) {
			console.log(data[i][0]);
			var item;
			if ( data[i][0] == "switch" ) {				
				item = this.create_switch(data[i][1], data[i][2], data[i][3], data[i][4]);
				main.append(item);
			} else if ( data[i][0] == "slider" ) {				
				item = this.create_slider(data[i][1], data[i][2], data[i][3], data[i][4], data[i][5], data[i][6]);
				main.append(item);
			} else if ( data[i][0] == "button" ) {				
				item = this.create_button(data[i][1], data[i][2], data[i][3], data[i][4]);
				main.append(item);
			}	
		}		
	}
		
	// Initiate operation
	this.load_start = function() {
				
		var hidden, visibilityState, visibilityChange;

		if (typeof document.hidden !== "undefined") {
			hidden = "hidden", visibilityChange = "visibilitychange", visibilityState = "visibilityState";
		} else if (typeof document.mozHidden !== "undefined") {
			hidden = "mozHidden", visibilityChange = "mozvisibilitychange", visibilityState = "mozVisibilityState";
		} else if (typeof document.msHidden !== "undefined") {
			hidden = "msHidden", visibilityChange = "msvisibilitychange", visibilityState = "msVisibilityState";
		} else if (typeof document.webkitHidden !== "undefined") {
			hidden = "webkitHidden", visibilityChange = "webkitvisibilitychange", visibilityState = "webkitVisibilityState";
		}

		var document_hidden = document[hidden];

		document.addEventListener(visibilityChange, function() {
			if(document_hidden != document[hidden]) {
				if(document[hidden]) {
					homeauto.hide();
				} else {
					homeauto.show();
				} 
				document_hidden = document[hidden];
			}
		});
		
		this.offline = false;
		$("#offline").hide();
		
		
		$("body").css("background-image", "url(D:/Projects/Web/jc2brown.ca/new/bkg/L" + (1 + Math.floor((6*Math.random()))) + ".png)");
		
		$.ajaxSetup({error:this.ajax_error});
		$.get("/load?", function(data) { 
			homeauto.load_handler(data); 
		});		
		
		this.show();
		
	}	
	
	
}


homeauto = new Homeauto();

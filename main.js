(function() {
	var serialPort = require("serialport");
	var SerialPort = serialPort.SerialPort;

	var server = require('child_process').spawn('node', ['server.js']);

	//console.log(process.argv);
	if(process.argv.length > 2) {
		var port = process.argv[2];
		console.log('Arduino : '+port);
	
	var sp = new SerialPort(port, {
		baudrate: 57600,
		parser: serialPort.parsers.readline('\n')
	});

	var clients = {};

	var server = require('socket.io')();
	server.on('connection', function(socket){});
	server.listen(3000);
	console.log("webclient : http://localhost:4000/index.html");

	function parseData(s) {
		var type = s.charAt(0);
		s = s.substring(1,s.length);
		
		// console.log(s);
		switch(type) {
			case 'U': // update
				var data = s.split(';').map(function(val) {
					return parseFloat(val);
				});
				server.emit('update', data);
			break;
			case 'D': // debug
				console.log('[debug] '+s);
			break;
		}
	}

	sp.on("data", function(data) {
		parseData(data.toString());
	});
	server.on("connection", function(client) {
		client.on('PID_motorV', function(data) {
			data = data.map(function(val) {
				return parseInt(val*1000);
			});
			// console.log(data);
			var str = ['P'].concat(data).join(';');
			console.log(str);
			sp.write(str+'\n');
		});
		client.on('PID_motorP', function(data) {
			data = data.map(function(val) {
				return parseInt(val*1000);
			});
			// console.log(data);
			var str = ['Q'].concat(data).join(';');
			console.log(str);
			sp.write(str+'\n');
		});
		client.on('consigne_V', function(data) {
			data = parseInt(data*1000);
			// console.log(data);
			var str = 'C;'+data;
			console.log(str);
			sp.write(str+'\n');
		});
		client.on('consigne_P', function(data) {
			data = parseInt(data*1000);
			// console.log(data);
			var str = 'D;'+data;
			console.log(str);
			sp.write(str+'\n');
		});
	});

	// setInterval(function() {
	// 	server.emit('update', [10,20,30,40]);
	// }, 1000/50);
	} else {
		console.log('Entrez un port serial en paramètres');
		serialPort.list(function(err, ports) {
			ports.forEach(function(port) {
				console.log(' - '+port.comName);
			});
			process.exit();
		});
	}
})();
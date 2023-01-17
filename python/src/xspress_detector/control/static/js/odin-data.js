
odin_data = {
  api_version: '0.1',
  ctrl_name: 'control',
  current_page: '.home-view',
  adapter_list: [],
  adapter_objects: {},
  ctrl_connected: false,
  fp_connected: [false,false,false,false],
  acq_id: '',
  daq: null
  };


String.prototype.replaceAll = function(search, replacement) {
    var target = this;
    return target.replace(new RegExp(search, 'g'), replacement);
};

$.put = function(url, data, callback, type)
{
  if ( $.isFunction(data) ){
    type = type || callback,
    callback = data,
    data = {}
  }

  return $.ajax({
    url: url,
    type: 'PUT',
    success: callback,
    data: data,
    contentType: type
  });
}

function render(url)
{
  // This function decides what type of page to show
  // depending on the current url hash value.
  // Get the keyword from the url.
  var temp = "." + url.split('/')[1];
  // Hide whatever page is currently shown.
  $(".page").hide();

  // Show the new page
  $(temp).show();
  odin_data.current_page = temp;

}


$( document ).ready(function() 
{
  $("#fp-tabs").tabs();
  update_api_version();
  update_api_adapters();
  render(decodeURI(window.location.hash));

  setInterval(update_api_version, 5000);
  setInterval(update_detector_status, 5000);
  // setInterval(update_fp_status, 1000);
  // setInterval(update_fr_status, 1000);
  // setInterval(update_meta_status, 1000);

  $('#set-hw-exposure').change(function(){
    update_exposure();
  });

  $('#set-hw-frames').change(function(){
    update_frames();
  });

  $('#set-hw-trigger-mode').on( "change", () => {
    console.log("trgger changed!!!");
    console.log("trgger changed!!!");
    console.log("trgger changed!!!");
    update_trigger_mode();
  } );

  $('#detector-arm-cmd').click(function(){
    ctrl_command('arm');
  });

  $('#detector-run-cmd').click(function(){
    ctrl_command('run');
  });

  $('#detector-stop-cmd').click(function(){
    ctrl_command('stop');
  });

  $('#detector-abort-cmd').click(function(){
    ctrl_command('abort');
  });

  $('#detector-dac-scan-cmd').click(function(){
    ctrl_command('dac_scan');
  });

  $('#detector-config-put-cmd').click(function(){
    detector_json_put_command();
  });

  $('#detector-config-get-cmd').click(function(){
    detector_json_get_command();
  });

  $('#fp-debug-level').change(function(){
    fp_debug_command();
    fr_debug_command();
  });

    $('#fp-config-cmd').click(function(){
    fp_configure_command();
  });

  $('#fp-start-cmd').click(function(){
    meta_start_command();
    fp_start_command();
  });

  $('#fp-stop-cmd').click(function(){
    fp_stop_command();
  });

  $('#fp-config-raw-mode-on').click(function(){
    fp_raw_mode_command();
  });

  $('#fp-config-raw-mode-off').click(function(){
    fp_process_mode_command();
  });

  $('#fr-reset-cmd').click(function(){
      fr_reset_statistics();
      fp_reset_statistics();
  });

  $(window).on('hashchange', function(){
    // On every hash change the render function is called with the new hash.
        // This is how the navigation of the app is executed.
        render(decodeURI(window.location.hash));
  });
});

function process_cmd_response(response)
{
}

function update_exposure() {
    set_value = $('#set-hw-exposure').val();
    $.put('/api/' + odin_data.api_version + '/' + odin_data.ctrl_name + '/xsp/exposure_time/' + set_value, process_cmd_response);
}

function update_frames() {
    set_value = $('#set-hw-frames').val();
    $.put('/api/' + odin_data.api_version + '/' + odin_data.ctrl_name + '/xsp/frames/' + set_value, process_cmd_response);
}

function update_trigger_mode() {
    const name = 'trigger_mode';
    const set_value = $('#set-hw-trigger-mode').find(":selected").val();
    // const value = TRIGGER_MODE[set_value];
    const data = JSON.stringify({trigger_mode : set_value})
    $.ajax(
      {
        url: '/api/' + odin_data.api_version + '/' + odin_data.ctrl_name + '/xsp/trigger_mode',
        type: 'PUT',
        success: () => {alert("sent trigger")},
        contentType: "application/json",
        data: data,
      }
    );

    // $.put('/api/' + odin_data.api_version + '/' + odin_data.ctrl_name + '/xsp/trigger_mode', {name : value}, process_cmd_response);
    // fp_mode_command(set_value);
}

function ctrl_command(command) {
    $.put('/api/' + odin_data.api_version + '/' + odin_data.ctrl_name + '/cmd/' + command, process_cmd_response);
}

function detector_json_put_command()
{
    try
    {
        value = JSON.stringify(JSON.parse($('#engineering-cmd').val()));
        $.ajax({
            url: '/api/' + odin_data.api_version + '/' + odin_data.ctrl_name,
            type: 'PUT',
            dataType: 'json',
            data: value,
            headers: {'Content-Type': 'application/json',
                      'Accept': 'application/json'},
            success: function(response){
                $('#engineering-rsp').val(JSON.stringify(JSON.parse(response['reply'])));
            },
            async: false
        });
    }
    catch(err)
    {
        alert("Failed to submit JSON PUT command.\nError: " + err.message);
    }
}

function detector_json_get_command()
{
    try
    {
        value = JSON.stringify(JSON.parse($('#engineering-cmd').val()));
        $.ajax({
            url: '/api/' + odin_data.api_version + '/' + odin_data.ctrl_name ,
            type: 'GET',
            dataType: 'json',
            data: value,
            headers: {'Content-Type': 'application/json',
                      'Accept': 'application/json'},
            success: function(response){
                $('#engineering-rsp').val(JSON.stringify(JSON.parse(response['reply'])));
            },
            async: false
        });
    }
    catch(err)
    {
        alert("Failed to submit JSON GET command.\nError: " + err.message);
    }
}

function send_meta_command(command, params)
{
    $.ajax({
        url: '/api/' + odin_data.api_version + '/meta_listener/config/' + command,
        type: 'PUT',
        dataType: 'json',
        data: params,
        headers: {'Content-Type': 'application/json',
            'Accept': 'application/json'},
        success: process_cmd_response,
        async: false
    });
}

function send_fp_command(command, params)
{
    $.ajax({
        url: '/api/' + odin_data.api_version + '/fp/config/' + command,
        type: 'PUT',
        dataType: 'json',
        data: params,
        headers: {'Content-Type': 'application/json',
            'Accept': 'application/json'},
        success: process_cmd_response,
        async: false
    });
}

function send_fr_command(command, params)
{
    $.ajax({
        url: '/api/' + odin_data.api_version + '/fr/config/' + command,
        type: 'PUT',
        dataType: 'json',
        data: params,
        headers: {'Content-Type': 'application/json',
            'Accept': 'application/json'},
        success: process_cmd_response,
        async: false
    });
}

function fr_reset_statistics()
{
    $.ajax({
        url: '/api/' + odin_data.api_version + '/fr/command/reset_statistics',
        type: 'PUT',
        dataType: 'json',
        headers: {'Content-Type': 'application/json',
            'Accept': 'application/json'},
        success: process_cmd_response,
        async: false
    });
}

function fp_reset_statistics()
{
    $.ajax({
        url: '/api/' + odin_data.api_version + '/fp/command/reset_statistics',
        type: 'PUT',
        dataType: 'json',
        headers: {'Content-Type': 'application/json',
            'Accept': 'application/json'},
        success: process_cmd_response,
        async: false
    });
}

function fp_debug_command() {
    send_fp_command('debug_level', $('#fp-debug-level').val());
}

function fr_debug_command() {
    send_fr_command('debug_level', $('#fp-debug-level').val());
}

function fp_mode_command(mode) {
    send_fp_command('xspress', JSON.stringify({
        "mode": mode.toLowerCase()
    }));
}

function meta_start_command() {
    send_fp_command('xspress', JSON.stringify({
        "acq_id": $('#set-fp-filename').val()
    }));
    odin_data.acq_id = $('#set-fp-filename').val();
    send_meta_command("acquisition_id", $('#set-fp-filename').val());
    send_meta_command("directory", $('#set-fp-path').val());
    send_meta_command("file_prefix", $('#set-fp-filename').val());
}

function fp_start_command() {
    send_fp_command('hdf', JSON.stringify({
        "acquisition_id": $('#set-fp-filename').val(),
        "file": {
            "name": $('#set-fp-filename').val(),
            "path": $('#set-fp-path').val()
        }
    }));
    send_fp_command('hdf', JSON.stringify({
        "write": true
    }));
}

function fp_raw_mode_command() {
    send_fp_command('xspress', JSON.stringify({
        "raw_mode": 1
    }));
}

function fp_process_mode_command() {
    send_fp_command('xspress', JSON.stringify({
        "raw_mode": 0
    }));
}

function fp_stop_command() {
    send_fp_command('hdf', JSON.stringify({
        "write": false
    }));
}

function update_api_version() {

    $.getJSON('/api', function(response) {
        $('#api-version').html(response.api);
        odin_data.api_version = response.api;
    });
}

function update_api_adapters() {
    $.getJSON('/api/' + odin_data.api_version + '/adapters/', function(response) {
        odin_data.adapter_list = response.adapters;
        adapter_list_html = response.adapters.join(", ");
        $('#api-adapters').html(adapter_list_html);
        //update_adapter_objects();
    });
}
const TRIGGER_MODE = {
  0: "software",
  1: "ttl_rising_edge",
  2: "burst",
  3: "ttl_veto_only",
  4: "software_start_stop",
  5: "idc",
  6: "ttl_both",
  7: "lvds_veto_only",
  8: "lvds_both",
  software: 0,
  ttl_rising_edge: 1,
  burst: 2,
  ttl_veto_only: 3,
  software_start_stop: 4,
  idc: 5,
  ttl_both: 6,
  lvds_veto_only: 7,
  lvds_both: 8,
};

function update_detector_status() {
    $.getJSON('/api/' + odin_data.api_version + '/' + odin_data.ctrl_name, function(response) {
        console.log(response);
        $('#detector-hw-username').html(response.adapter.username);
        $('#detector-hw-start-time').html(response.adapter.start_time);
        $('#detector-hw-up-time').html(response.adapter.up_time);
        $('#detector-hw-endpoint').html(response.ctrl_endpoint);
        $('#detector-hw-connected').html(led_html(''+response['connected'], 'green', 26));
        $('#detector-hw-exposure').html(response.xsp.exposure_time);
        $('#detector-hw-frames').html(response.xsp.frames);
        $('#detector-hw-trigger-mode').html(response.xsp.trigger_mode);
    });

    $.getJSON('/api/' + odin_data.api_version + '/' + odin_data.ctrl_name + '/config/frames_per_trigger', function(response) {
        if (odin_data.ctrl_connected){
            $('#detector-hw-frames-per-trigger').html(response['value']);
        }
    });
    $.getJSON('/api/' + odin_data.api_version + '/' + odin_data.ctrl_name + '/config/mode', function(response) {
        if (odin_data.ctrl_connected){
            $('#detector-hw-mode').html(response['value']);
        }
    });
    $.getJSON('/api/' + odin_data.api_version + '/' + odin_data.ctrl_name + '/config/profile', function(response) {
        if (odin_data.ctrl_connected){
            $('#detector-hw-profile').html(response['value']);
        }
    });
    $.getJSON('/api/' + odin_data.api_version + '/' + odin_data.ctrl_name + '/status/state', function(response) {
        if (odin_data.ctrl_connected){
            $('#detector-hw-state').html(response['value']);
        }
    });
}

function update_fp_status() {
    //$.getJSON('/api/' + odin_data.api_version + '/fp/endpoint', function(response) {
    //    //alert(response.endpoint);
    //    $('#fp-endpoint').html(response.endpoint);
    //});
    $.getJSON('/api/' + odin_data.api_version + '/fp/status/connected', function(response) {
        //alert(response['value']);
        var no_of_fps = response['value'].length;
        if (odin_data.daq == null){
            odin_data.daq = new DAQHolder('xspress-daq-container', no_of_fps);
            odin_data.daq.init();
        } else {
            if (no_of_fps != odin_data.daq.get_no_of_fps()){
                odin_data.daq = new DAQHolder('xspress-daq-container', no_of_fps);
                odin_data.daq.init();
            }
        }
        odin_data.daq.setFPConnected(response['value']);
    });
    $.getJSON('/api/' + odin_data.api_version + '/fp/status/hdf/processes', function(response) {
        odin_data.daq.setFPProcesses(response['value']);
    });
    $.getJSON('/api/' + odin_data.api_version + '/fp/status/hdf/rank', function(response) {
        odin_data.daq.setFPRank(response['value']);
    });
    $.getJSON('/api/' + odin_data.api_version + '/fp/status/hdf/writing', function(response) {
        odin_data.daq.setFPWriting(response['value']);
    });
    $.getJSON('/api/' + odin_data.api_version + '/fp/status/hdf/frames_written', function(response) {
        odin_data.daq.setFPValuesWritten(response['value']);
    });
    $.getJSON('/api/' + odin_data.api_version + '/fp/status/xspress/invalid_packets', function(response) {
        odin_data.daq.setFPInvalidPackets(response['value']);
    });
    $.getJSON('/api/' + odin_data.api_version + '/fp/status/xspress/timestamp_mismatches', function(response) {
        odin_data.daq.setFPTimestampMismatches(response['value']);
    });
    $.getJSON('/api/' + odin_data.api_version + '/fp/status/xspress/processed_jobs', function(response) {
        odin_data.daq.setFPPacketsProcessed(response['value']);
    });
    $.getJSON('/api/' + odin_data.api_version + '/fp/status/xspress/job_queue', function(response) {
        odin_data.daq.setFPJobQueueSize(response['value']);
    });
    $.getJSON('/api/' + odin_data.api_version + '/fp/status/xspress/results_queue', function(response) {
        odin_data.daq.setFPResultsQueueSize(response['value']);
    });
    $.getJSON('/api/' + odin_data.api_version + '/fp/status/xspress/processed_frames', function(response) {
        odin_data.daq.setFPProcessedFrames(response['value']);
    });
    $.getJSON('/api/' + odin_data.api_version + '/fp/status/xspress/output_frames', function(response) {
        odin_data.daq.setFPOutputFrames(response['value']);
    });
    $.getJSON('/api/' + odin_data.api_version + '/fp/config/xspress/mode', function(response) {
        odin_data.daq.setFPOperationalMode(response['value']);
    });
    $.getJSON('/api/' + odin_data.api_version + '/fp/config/xspress/raw_mode', function(response) {
        odin_data.daq.setFPRawMode(response['value']);
    });
}

function update_fr_status() {
    $.getJSON('/api/' + odin_data.api_version + '/fr/status/buffers/empty', function (response) {
        //alert(JSON.stringify(response));
        odin_data.daq.setFREmptyBuffers(response['value']);
    });

    $.getJSON('/api/' + odin_data.api_version + '/fr/status/decoder/packets', function (response) {
        odin_data.daq.setFRPackets(response['value']);
        total_pkts = 0;
        for (var index = 0; index < response['value'].length; index++){
            total_pkts += parseInt(response['value'][index]);
        }
        $('#fr-pkts-received').html(''+total_pkts);
    });
    $.getJSON('/api/' + odin_data.api_version + '/fr/status/connected', function(response) {
        odin_data.daq.setFRConnected(response['value']);
    });
}

function update_meta_status() {
    $.getJSON('/api/' + odin_data.api_version + '/meta_listener/status', function (response) {
        data = response['value'][0];
        //alert(JSON.stringify(data));
        //alert(JSON.stringify(odin_data.acq_id));
        $('#meta-init').html(led_html(data['connected'], 'green', 26));
        if ($.isEmptyObject(data['acquisitions'])){
          $('#meta-active').html(led_html(false, 'green', 26));
          $('#meta-writing').html(led_html(false, 'green', 26));
          $('#meta-out').html('');
          $('#meta-frames').html('0');
          $('#meta-writers').html('0');
        } else {
          $('#meta-active').html(led_html(true, 'green', 26));
          $('#meta-writing').html(led_html(data['acquisitions'][odin_data.acq_id]['writing'], 'green', 26));
          $('#meta-out').html(data['acquisitions'][odin_data.acq_id]['filename']);
          $('#meta-frames').html(data['acquisitions'][odin_data.acq_id]['written']);
          $('#meta-writers').html(data['acquisitions'][odin_data.acq_id]['num_processors']);
        }
    });
}

function led_html(value, colour, width)
{
  var html_text = "<img width=" + width + "px src=img/";
  if (value == 'true' || value === true){
    html_text +=  colour + "-led-on";
  } else {
    html_text += "led-off";
  }
  html_text += ".png></img>";
  return html_text;
}


class DAQTab {
    // This class holds all information relating to a single DAQ tab
    // qty x FR status
    // qty X FP status
    constructor(owner, tab_number, qty){
        // Create a table for 8 application sets
        this.tab_number = tab_number;
        this.qty = qty;
        this.startfp = 1 + ((tab_number-1)*8);
        this.endfp = 8 + ((tab_number-1)*8);
        this.data_fr_connected = new Array();
        this.data_fp_connected = new Array();
        this.data_fr_packets = new Array();
        this.data_fr_empty_buffers = new Array();
        this.data_fp_processes = new Array();
        this.data_fp_rank = new Array();
        this.data_fp_pkts_processed = new Array();
        this.data_fp_job_q = new Array();
        this.data_fp_result_q = new Array();
        this.data_fp_invalid_pkt = new Array();
        this.data_fp_ts_mm = new Array();
        this.data_fp_proc_frames = new Array();
        this.data_fp_out_frames = new Array();
        this.data_fp_writing = new Array();
        this.data_fp_written = new Array();
        this.data_fp_raw_mode = new Array();
        this.data_fp_op_mode = new Array();
        this.title = "DAQ (FRs and FPs) ["+this.startfp+" - "+this.endfp+"]<BR><BR>";
        if (tab_number == 0){
            this.title = "Overview<BR><BR>";
        }
        owner.html(this.title);
        this.table = $('<table></table>');
        this.rows = new Array();
        this.desc = {
            'fr-connected': 'Frame Receiver',
            'fp-connected': 'Frame Processor',
            'fr-packets': 'Packets Received',
            'fr-empty-buffers': 'Empty Buffers',
            'fp-processes': 'Processes',
            'fp-rank': 'Rank',
            'fp-pkts-processed': 'Packets Processed',
            'fp-job-q': 'Job Queue Size',
            'fp-result-q': 'Results Queue Size',
            'fp-invalid-pkt': 'Invalid Packets',
            'fp-ts-mm': 'Timestamp Mismatches',
            'fp-proc-frames': 'Processed Frames',
            'fp-out-frames': 'Output Frames',
            'fp-writing': 'Writing',
            'fp-written': 'Values Written',
            'fp-raw-mode': 'Decode Mode',
            'fp-op-mode': 'Operational Mode'
        }
        for (var index in this.desc){
            var row = $('<tr></tr>');
            var td = $('<td width="200px" align="left">' + this.desc[index] + '</td>');
            row.append(td);
            for (var apn = 0; apn < qty; apn++){
                var td = $('<td width="200px" align="center" id="' + index + '-' + tab_number + '-' + apn + '"></td>');
                row.append(td);
            }
            this.rows.push(row);
            this.table.append(row);
        }
        owner.append(this.table);
    }

    init(){
        for (var index = 0; index < this.qty; index++){
            this.setFRConnected(index, false);
            this.setFPConnected(index, false);
            this.setFRPackets(index, 0);
            this.setFREmptyBuffers(index, 0);
            this.setFPProcesses(index, 0);
            this.setFPRank(index, 0);
            this.setFPPacketsProcessed(index, 0);
            this.setFPJobQueueSize(index, 0);
            this.setFPResultsQueueSize(index, 0);
            this.setFPInvalidPackets(index, 0);
            this.setFPTimestampMismatches(index, 0);
            this.setFPProcessedFrames(index, 0);
            this.setFPOutputFrames(index, 0);
            this.setFPWriting(index, false);
            this.setFPValuesWritten(index, 0);
            this.setFPRawMode(index, 0);
            this.setFPOperationalMode(index, 0);
        }
    }

    fr_disconnected(index){
        this.setFRPackets(index, 0);
        this.setFREmptyBuffers(index, 0);
    }

    fp_disconnected(index){
        this.setFPProcesses(index, 0);
        this.setFPRank(index, 0);
        this.setFPPacketsProcessed(index, 0);
        this.setFPJobQueueSize(index, 0);
        this.setFPResultsQueueSize(index, 0);
        this.setFPInvalidPackets(index, 0);
        this.setFPTimestampMismatches(index, 0);
        this.setFPProcessedFrames(index, 0);
        this.setFPOutputFrames(index, 0);
        this.setFPWriting(index, false);
        this.setFPValuesWritten(index, 0);
        this.setFPRawMode(index, 0);
        this.setFPOperationalMode(index, 0);
    }

    setFRConnected(id, connected){
        this.data_fr_connected[id] = connected;
        var ss = '#fr-connected-' + this.tab_number + '-' + id;
        $(ss).html(led_html(connected, 'green', 26));
        if (connected == 'true' || connected === true){
        } else {
            this.fr_disconnected();
        }
    }

    getAllFRConnected(){
        // Loop over each checking for any not connected
        var connected = true;
        for (var index = 0; index < this.data_fr_connected.length; index++){
            if (this.data_fr_connected[index] == false){
                connected = false;
            }
        }
        return connected;
    }

    setFPConnected(id, connected){
        this.data_fp_connected[id] = connected;
        if (connected == 'false' || connected === false){
            this.fp_disconnected();
        }
        var ss = '#fp-connected-' + this.tab_number + '-' + id;
        $(ss).html(led_html(connected, 'green', 26));
    }

    getAllFPConnected(){
        // Loop over each checking for any not connected
        var connected = true;
        for (var index = 0; index < this.data_fr_connected.length; index++){
            if (this.data_fp_connected[index] == false){
                connected = false;
            }
        }
        return connected;
    }

    setFRPackets(id, packets){
        this.data_fr_packets[id] = packets;
        var ss = '#fr-packets-' + this.tab_number + '-' + id;
        $(ss).html(''+packets);
    }

    getAllFRPackets(){
        var packets = 0;
        for (var index = 0; index < this.data_fr_packets.length; index++){
            packets += this.data_fr_packets[index];
        }
        return packets;
    }

    setFREmptyBuffers(id, buffers){
        this.data_fr_empty_buffers[id] = buffers;
        var ss = '#fr-empty-buffers-' + this.tab_number + '-' + id;
        $(ss).html(''+buffers);
    }

    getAllFREmptyBuffers(){
        var buffers = 0;
        for (var index = 0; index < this.data_fr_empty_buffers.length; index++){
            buffers += this.data_fr_empty_buffers[index];
        }
        return buffers;
    }

    setFPProcesses(id, processes){
        this.data_fp_processes[id] = processes;
        var ss = '#fp-processes-' + this.tab_number + '-' + id;
        $(ss).html(''+processes);
    }

    getAllFPProcesses(){
        var processes = this.data_fp_processes[0];
        for (var index = 0; index < this.data_fp_processes.length; index++){
            if (processes != this.data_fp_processes[index]){
                return "Inconsistent";
            }
        }
        return processes;
    }

    setFPRank(id, rank){
        this.data_fp_rank[id] = rank;
        var ss = '#fp-rank-' + this.tab_number + '-' + id;
        $(ss).html(''+rank);
    }

    getAllFPRank(){
        var f_rank = this.data_fp_rank[0];
        var l_rank = this.data_fp_rank[this.data_fp_rank.length - 1];
        return f_rank + '-' + l_rank;
    }

    setFPPacketsProcessed(id, pkts){
        this.data_fp_pkts_processed[id] = pkts;
        var ss = '#fp-pkts-processed-' + this.tab_number + '-' + id;
        $(ss).html(''+pkts);
    }

    getAllFPPacketsProcessed(){
        var pkts = 0;
        for (var index = 0; index < this.data_fp_pkts_processed.length; index++){
            pkts += this.data_fp_pkts_processed[index];
        }
        return pkts;
    }

    setFPJobQueueSize(id, size){
        this.data_fp_job_q[id] = size;
        var ss = '#fp-job-q-' + this.tab_number + '-' + id;
        $(ss).html(''+size);
    }

    getAllFPJobQueueSize(){
        var queue = 0;
        for (var index = 0; index < this.data_fp_job_q.length; index++){
            queue += this.data_fp_job_q[index];
        }
        return queue;
    }

    setFPResultsQueueSize(id, size){
        this.data_fp_result_q[id] = size;
        var ss = '#fp-result-q-' + this.tab_number + '-' + id;
        $(ss).html(''+size);
    }

    getAllFPResultsQueueSize(){
        var queue = 0;
        for (var index = 0; index < this.data_fp_result_q.length; index++){
            queue += this.data_fp_result_q[index];
        }
        return queue;
    }

    setFPInvalidPackets(id, pkts){
        this.data_fp_invalid_pkt[id] = pkts;
        var ss = '#fp-invalid-pkt-' + this.tab_number + '-' + id;
        $(ss).html(''+pkts);
    }

    getAllFPInvalidPackets(){
        var pkts = 0;
        for (var index = 0; index < this.data_fp_invalid_pkt.length; index++){
            pkts += this.data_fp_invalid_pkt[index];
        }
        return pkts;
    }

    setFPTimestampMismatches(id, tsmm){
        this.data_fp_ts_mm[id] = tsmm;
        var ss = '#fp-ts-mm-' + this.tab_number + '-' + id;
        $(ss).html(''+tsmm);
    }

    getAllFPTimestampMismatches(){
        var tsmm = 0;
        for (var index = 0; index < this.data_fp_ts_mm.length; index++){
            tsmm += this.data_fp_ts_mm[index];
        }
        return tsmm;
    }

    setFPProcessedFrames(id, frames){
        this.data_fp_proc_frames[id] = frames;
        var ss = '#fp-proc-frames-' + this.tab_number + '-' + id;
        $(ss).html(''+frames);
    }

    getAllFPProcessedFrames(){
        var frames = 0;
        for (var index = 0; index < this.data_fp_proc_frames.length; index++){
            frames += this.data_fp_proc_frames[index];
        }
        return frames;
    }

    setFPOutputFrames(id, frames){
        this.data_fp_out_frames[id] = frames;
        var ss = '#fp-out-frames-' + this.tab_number + '-' + id;
        $(ss).html(''+frames);
    }

    getAllFPOutputFrames(){
        var frames = 0;
        for (var index = 0; index < this.data_fp_out_frames.length; index++){
            frames += this.data_fp_out_frames[index];
        }
        return frames;
    }

    setFPWriting(id, writing){
        this.data_fp_writing[id] = writing;
        var ss = '#fp-writing-' + this.tab_number + '-' + id;
        $(ss).html(led_html(writing, 'green', 26));
    }

    getAllFPWriting(){
        var writing = true;
        for (var index = 0; index < this.data_fp_writing.length; index++){
            if (this.data_fp_writing[index] == false){
                writing = false;
            }
        }
        return writing;
    }

    setFPValuesWritten(id, values){
        this.data_fp_written[id] = values;
        var ss = '#fp-written-' + this.tab_number + '-' + id;
        $(ss).html(''+values);
    }

    getAllFPValuesWritten(){
        var frames = 0;
        for (var index = 0; index < this.data_fp_written.length; index++){
            frames += this.data_fp_written[index];
        }
        return frames;
    }

    setFPRawMode(id, mode){
        this.data_fp_raw_mode[id] = mode;
        var ss = '#fp-raw-mode-' + this.tab_number + '-' + id;
        $(ss).html(''+mode);
    }

    getAllFPRawMode(){
        var mode = this.data_fp_raw_mode[0];
        for (var index = 0; index < this.data_fp_raw_mode.length; index++){
            if (mode != this.data_fp_raw_mode[index]){
                return "Inconsistent";
            }
        }
        return mode;
    }

    setFPOperationalMode(id, mode){
        this.data_fp_op_mode[id] = mode;
        var ss = '#fp-op-mode-' + this.tab_number + '-' + id;
        $(ss).html(''+mode);
    }

    getAllFPOperationalMode(){
        var mode = this.data_fp_op_mode[0];
        for (var index = 0; index < this.data_fp_op_mode.length; index++){
            if (mode != this.data_fp_op_mode[index]){
                return "Inconsistent";
            }
        }
        return mode;
    }
}

class DAQHolder {
    constructor(owner, no_of_fps){
        // Number of tabs required = number of fps / 8 + 1
        this.no_of_fps = no_of_fps;
        this.daqtabs = new Array();
        if (no_of_fps % 10 == 0){
            this.fp_batch_count = 10;
        } else {
            this.fp_batch_count = 8;
        }
        this.tab_count = Math.ceil(no_of_fps / this.fp_batch_count) + 1;
        this.tabs = $('<div id="xspress-daq-tabs"></div>');
        this.list = $('<ul class="nav nav-tabs"></ul>');
        this.bodies = $('<div class="tab-content"></div>');
        this.tabs.append(this.list);
        this.tabs.append(this.bodies);
        for (var index = 0; index < this.tab_count; index++){
            var startfp = 1 + ((index-1)*this.fp_batch_count);
            var endfp = this.fp_batch_count + ((index-1)*this.fp_batch_count);
            var tabtitle = "FR/FP [" + startfp + "-" + endfp + "]";
            if (index == 0){
                tabtitle = "Overview";
            }
            var tabheader = $('<li class="nav-item waves-effect waves-light"><a class="nav-link" href="#xspress-daq-tab-' + index + '" data-toggle="tab">' + tabtitle + '</a></li>');
            this.list.append(tabheader);
            var tabbody = $('<div class="tab-pane" id="xspress-daq-tab-' + index + '"></div>');
            this.bodies.append(tabbody);
            if (index == 0){
                this.maintab = new DAQTab(tabbody, index, (this.tab_count-1));
            } else {
                var daqtab = new DAQTab(tabbody, index, this.fp_batch_count);
                this.daqtabs.push(daqtab);
            }
        }
        $('#' + owner).html('');
        $('#' + owner).append(this.tabs);
    }

    get_no_of_fps(){
        return this.no_of_fps;
    }

    init(){
        this.maintab.init();
        for (var tab of this.daqtabs){
            tab.init();
        }
        $('.nav-tabs a[href="#xspress-daq-tab-0"]').tab('show');
    }

    setFRConnected(connected){
        // Loop over the array, calling the correct indexed tab item
        for (var index = 0; index < connected.length; index++){
            var tab_index = Math.floor(index / this.fp_batch_count);
            var id_index = index - (tab_index * this.fp_batch_count);
            this.daqtabs[tab_index].setFRConnected(id_index, connected[index]);
        }
        for (var index = 0; index < this.daqtabs.length; index++){
            this.maintab.setFRConnected(index, this.daqtabs[index].getAllFRConnected());
        }
    }

    setFPConnected(connected){
        // Loop over the array, calling the correct indexed tab item
        for (var index = 0; index < connected.length; index++){
            var tab_index = Math.floor(index / this.fp_batch_count);
            var id_index = index - (tab_index * this.fp_batch_count);
            this.daqtabs[tab_index].setFPConnected(id_index, connected[index]);
        }
        for (var index = 0; index < this.daqtabs.length; index++){
            this.maintab.setFPConnected(index, this.daqtabs[index].getAllFPConnected());
        }
    }

    setFRPackets(packets){
        // Loop over the array, calling the correct indexed tab item
        for (var index = 0; index < packets.length; index++){
            var tab_index = Math.floor(index / this.fp_batch_count);
            var id_index = index - (tab_index * this.fp_batch_count);
            this.daqtabs[tab_index].setFRPackets(id_index, packets[index]);
        }
        for (var index = 0; index < this.daqtabs.length; index++){
            this.maintab.setFRPackets(index, this.daqtabs[index].getAllFRPackets());
        }
    }

    setFREmptyBuffers(buffers){
        // Loop over the array, calling the correct indexed tab item
        for (var index = 0; index < buffers.length; index++){
            var tab_index = Math.floor(index / this.fp_batch_count);
            var id_index = index - (tab_index * this.fp_batch_count);
            this.daqtabs[tab_index].setFREmptyBuffers(id_index, buffers[index]);
        }
        for (var index = 0; index < this.daqtabs.length; index++){
            this.maintab.setFREmptyBuffers(index, this.daqtabs[index].getAllFREmptyBuffers());
        }
    }

    setFPProcesses(processes){
        // Loop over the array, calling the correct indexed tab item
        for (var index = 0; index < processes.length; index++){
            var tab_index = Math.floor(index / this.fp_batch_count);
            var id_index = index - (tab_index * this.fp_batch_count);
            this.daqtabs[tab_index].setFPProcesses(id_index, processes[index]);
        }
        for (var index = 0; index < this.daqtabs.length; index++){
            this.maintab.setFPProcesses(index, this.daqtabs[index].getAllFPProcesses());
        }
    }

    setFPRank(rank){
        // Loop over the array, calling the correct indexed tab item
        for (var index = 0; index < rank.length; index++){
            var tab_index = Math.floor(index / this.fp_batch_count);
            var id_index = index - (tab_index * this.fp_batch_count);
            this.daqtabs[tab_index].setFPRank(id_index, rank[index]);
        }
        for (var index = 0; index < this.daqtabs.length; index++){
            this.maintab.setFPRank(index, this.daqtabs[index].getAllFPRank());
        }
    }

    setFPPacketsProcessed(pkts){
        // Loop over the array, calling the correct indexed tab item
        for (var index = 0; index < pkts.length; index++){
            var tab_index = Math.floor(index / this.fp_batch_count);
            var id_index = index - (tab_index * this.fp_batch_count);
            this.daqtabs[tab_index].setFPPacketsProcessed(id_index, pkts[index]);
        }
        for (var index = 0; index < this.daqtabs.length; index++){
            this.maintab.setFPPacketsProcessed(index, this.daqtabs[index].getAllFPPacketsProcessed());
        }
    }

    setFPJobQueueSize(size){
        // Loop over the array, calling the correct indexed tab item
        for (var index = 0; index < size.length; index++){
            var tab_index = Math.floor(index / this.fp_batch_count);
            var id_index = index - (tab_index * this.fp_batch_count);
            this.daqtabs[tab_index].setFPJobQueueSize(id_index, size[index]);
        }
        for (var index = 0; index < this.daqtabs.length; index++){
            this.maintab.setFPJobQueueSize(index, this.daqtabs[index].getAllFPJobQueueSize());
        }
    }

    setFPResultsQueueSize(size){
        // Loop over the array, calling the correct indexed tab item
        for (var index = 0; index < size.length; index++){
            var tab_index = Math.floor(index / this.fp_batch_count);
            var id_index = index - (tab_index * this.fp_batch_count);
            this.daqtabs[tab_index].setFPResultsQueueSize(id_index, size[index]);
        }
        for (var index = 0; index < this.daqtabs.length; index++){
            this.maintab.setFPResultsQueueSize(index, this.daqtabs[index].getAllFPResultsQueueSize());
        }
    }

    setFPInvalidPackets(pkts){
        // Loop over the array, calling the correct indexed tab item
        for (var index = 0; index < pkts.length; index++){
            var tab_index = Math.floor(index / this.fp_batch_count);
            var id_index = index - (tab_index * this.fp_batch_count);
            this.daqtabs[tab_index].setFPInvalidPackets(id_index, pkts[index]);
        }
        for (var index = 0; index < this.daqtabs.length; index++){
            this.maintab.setFPInvalidPackets(index, this.daqtabs[index].getAllFPInvalidPackets());
        }
    }

    setFPTimestampMismatches(tsmm){
        // Loop over the array, calling the correct indexed tab item
        for (var index = 0; index < tsmm.length; index++){
            var tab_index = Math.floor(index / this.fp_batch_count);
            var id_index = index - (tab_index * this.fp_batch_count);
            this.daqtabs[tab_index].setFPTimestampMismatches(id_index, tsmm[index]);
        }
        for (var index = 0; index < this.daqtabs.length; index++){
            this.maintab.setFPTimestampMismatches(index, this.daqtabs[index].getAllFPTimestampMismatches());
        }
    }

    setFPProcessedFrames(frames){
        // Loop over the array, calling the correct indexed tab item
        for (var index = 0; index < frames.length; index++){
            var tab_index = Math.floor(index / this.fp_batch_count);
            var id_index = index - (tab_index * this.fp_batch_count);
            this.daqtabs[tab_index].setFPProcessedFrames(id_index, frames[index]);
        }
        for (var index = 0; index < this.daqtabs.length; index++){
            this.maintab.setFPProcessedFrames(index, this.daqtabs[index].getAllFPProcessedFrames());
        }
    }

    setFPOutputFrames(frames){
        // Loop over the array, calling the correct indexed tab item
        for (var index = 0; index < frames.length; index++){
            var tab_index = Math.floor(index / this.fp_batch_count);
            var id_index = index - (tab_index * this.fp_batch_count);
            this.daqtabs[tab_index].setFPOutputFrames(id_index, frames[index]);
        }
        for (var index = 0; index < this.daqtabs.length; index++){
            this.maintab.setFPOutputFrames(index, this.daqtabs[index].getAllFPOutputFrames());
        }
    }

    setFPWriting(writing){
        // Loop over the array, calling the correct indexed tab item
        for (var index = 0; index < writing.length; index++){
            var tab_index = Math.floor(index / this.fp_batch_count);
            var id_index = index - (tab_index * this.fp_batch_count);
            this.daqtabs[tab_index].setFPWriting(id_index, writing[index]);
        }
        for (var index = 0; index < this.daqtabs.length; index++){
            this.maintab.setFPWriting(index, this.daqtabs[index].getAllFPWriting());
        }
    }

    setFPValuesWritten(values){
        // Loop over the array, calling the correct indexed tab item
        for (var index = 0; index < values.length; index++){
            var tab_index = Math.floor(index / this.fp_batch_count);
            var id_index = index - (tab_index * this.fp_batch_count);
            this.daqtabs[tab_index].setFPValuesWritten(id_index, values[index]);
        }
        for (var index = 0; index < this.daqtabs.length; index++){
            this.maintab.setFPValuesWritten(index, this.daqtabs[index].getAllFPValuesWritten());
        }
    }

    setFPRawMode(mode){
        // Loop over the array, calling the correct indexed tab item
        for (var index = 0; index < mode.length; index++){
            var tab_index = Math.floor(index / this.fp_batch_count);
            var id_index = index - (tab_index * this.fp_batch_count);
            this.daqtabs[tab_index].setFPRawMode(id_index, mode[index]);
        }
        for (var index = 0; index < this.daqtabs.length; index++){
            this.maintab.setFPRawMode(index, this.daqtabs[index].getAllFPRawMode());
        }
    }

    setFPOperationalMode(mode){
        // Loop over the array, calling the correct indexed tab item
        for (var index = 0; index < mode.length; index++){
            var tab_index = Math.floor(index / this.fp_batch_count);
            var id_index = index - (tab_index * this.fp_batch_count);
            this.daqtabs[tab_index].setFPOperationalMode(id_index, mode[index]);
        }
        for (var index = 0; index < this.daqtabs.length; index++){
            this.maintab.setFPOperationalMode(index, this.daqtabs[index].getAllFPOperationalMode());
        }
    }
}

function create_tabs(owner, no_of_fps)
{
    // Number of tabs required = number of fps / 8 + 1
    fp_batch_count = 8;
    if (no_of_fps % 10 == 0){
        fp_batch_count = 10;
    }
    tab_count = Math.ceil(no_of_fps / fp_batch_count) + 1;
    var tabs = $('<div id="xspress-daq-tabs"></div>');
    var list = $('<ul class="nav nav-tabs"></ul>');
    var bodies = $('<div class="tab-content"></div>');
    tabs.append(list);
    tabs.append(bodies);
    for (index = 0; index < tab_count; index++){
        var tabheader = $('<li class="nav-item waves-effect waves-light"><a class="nav-link" href="#xspress-daq-tab-' + index + '" data-toggle="tab">Tab ' + index + '</a></li>');
        list.append(tabheader);
        var tabbody = $('<div class="tab-pane" id="xspress-daq-tab-' + index + '">TEST TEST TEST '+index+'</div>');
        bodies.append(tabbody);
        var dqtab = new DAQTab(tabbody, index);
    }
    $('#' + owner).append(tabs);
    dqtab.setFRConnected(0, false);
    dqtab.setFRConnected(1, true);
}

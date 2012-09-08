/* minimal looping
 * -- rmo@NUMM.ORG
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include <jack/jack.h>

jack_client_t *client;
jack_port_t* input_port;

/* 100 seconds should be plenty of time, and at ~5mb not a big deal to
   allocate for the first loop in lieu of a more complex dynamic
   memory management model */
const jack_nframes_t MAX_RECORDING_LENGTH = 44100 * 100;

/* We set this after the first recording to determine the length of
   all subsequent recordings. */
int recording_length = 0;
int nloops = 0;

/* State maintained within `process' callback. */
volatile jack_nframes_t cur_frame = 0;
volatile int is_recording = 0;

jack_port_t** output_ports;
jack_default_audio_sample_t** loops;

static void initialize_new_loop(int nsamples) {

  /* Increase the size of the output_ports and loops arrays by one */
  /* XXX: These should be linked-list structs */

  jack_port_t** tmp_output_ports = (jack_port_t **) malloc((nloops + 1) * sizeof(jack_port_t *));
  jack_default_audio_sample_t** tmp_loops = (jack_default_audio_sample_t **) malloc((nloops + 1) * sizeof(jack_default_audio_sample_t *));

  int i;
  for(i=0; i<nloops; ++i) {
    tmp_output_ports[i] = output_ports[i];
    tmp_loops[i] = loops[i];
  }
  char loop_name[64];
  sprintf(loop_name, "loop_%d", nloops+1);

  tmp_output_ports[nloops] = jack_port_register (client, loop_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
  tmp_loops[nloops] = (jack_default_audio_sample_t *) malloc(nsamples * sizeof(jack_default_audio_sample_t));

  if(nloops) {
    free(output_ports);
    free(loops);
  }

  output_ports = tmp_output_ports;
  loops = tmp_loops;

  ++nloops;
}

static int process(jack_nframes_t nframes, void *arg) {
  int S = sizeof(jack_default_audio_sample_t);
  int cur_loop = nloops - 1;

  /* The first loop can be much longer than others. */
  jack_nframes_t loop_nframes = (cur_loop || (recording_length && !is_recording)) ? recording_length : MAX_RECORDING_LENGTH;

  if(is_recording) {
    jack_default_audio_sample_t* input = jack_port_get_buffer (input_port, nframes);

    if(cur_frame + nframes <= loop_nframes) {
      /* mid-loop */
      memcpy(&loops[cur_loop][cur_frame], input, nframes * S);
    }
    else {
      /* wrap around to beginning of loop */
      memcpy(&loops[cur_loop][cur_frame], input, (loop_nframes-cur_frame)*S);
      memcpy(loops[cur_loop], &input[loop_nframes-cur_frame], (nframes - (loop_nframes-cur_frame))*S);
    }
  }

  /* playback all of the loops */
  int i;
  for(i=0; i<nloops; ++i) {
    jack_default_audio_sample_t* output = jack_port_get_buffer(output_ports[i], nframes);

    if(cur_frame + nframes <= loop_nframes) {
      memcpy(output, &loops[i][cur_frame], nframes*S);
    }
    else {
      /* Wrap around. NB: loops must be as long as jack buffers! */
      memcpy(output, &loops[i][cur_frame], (loop_nframes-cur_frame)*S);
      memcpy(&output[loop_nframes-cur_frame], &loops[i][0], (nframes - (loop_nframes-cur_frame))*S);
    }
  }

  cur_frame += nframes;
  if(cur_frame > loop_nframes) {
    cur_frame = cur_frame % loop_nframes;
    fprintf(stdout, "loop\n");
  }

  return 0;
}

static void quit(int sig) {
  jack_client_close(client);

  int i;
  for(i=0; i<nloops; ++i) {
    free(loops[i]);
  }

  fprintf(stderr, "signal received, exiting ...\n");
  exit(0);
}

int main (int argc, char *argv[]) {
  jack_status_t status;

  if(!(client = jack_client_open("LOO.C", JackNoStartServer, &status))) {
    fprintf(stderr, "jack server not running\n");
    return 1;
  }

  jack_set_process_callback (client, process, 0);

  initialize_new_loop(MAX_RECORDING_LENGTH);

  if(jack_activate(client)) {
    fprintf(stderr, "cannot activate client, whatever that means\n");
    return 1;
  }

  if(!(input_port = jack_port_register (client, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0))) {
    fprintf(stderr, "cannot register input port, for some reason\n");
    return 1;
  }

  /* gracefully quit, upon kindly request */
  /* XXX: apparently `signal' is deprecated for sigaction (?) */
  /* XXX: also, WIN32 may use different signals (?) */
  signal(SIGQUIT, quit);
  signal(SIGTERM, quit);
  signal(SIGHUP, quit);
  signal(SIGINT, quit);

  int c;
  while (1) {
    c = getchar();
    
    switch (c) {
    case 10:                    /* newline toggles recording */
      if(!is_recording) {
        fprintf(stderr, "start recording\n");
        if(nloops == 1) {
          /* The first loop should start at the beginning */
          cur_frame=0;
        }
        is_recording=1;
      }
      else {
        fprintf(stderr, "stop recording\n");
        if(nloops == 1) {
          recording_length = cur_frame;
          cur_frame = 0;
        }
        is_recording = 0;
      }
      break;
    case 'n':
      /* Create a new loop */
      if(recording_length > 0) {
        if(nloops == 1) {
          cur_frame = 0;
        }
        initialize_new_loop(recording_length);
        fprintf(stderr, "new loop\n");
      }
      else {
        fprintf(stderr, "error: record a base loop first (think fast!)\n");
      }
      break;
    }
  };

  /* how can we ever get here? */
  fprintf(stderr, "really? you found me here?\n");
  jack_client_close(client);
}

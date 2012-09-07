#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include <jack/jack.h>
/* #include <jack/transport.h> */

jack_client_t *client;
jack_port_t* input_port;

/* XXX: Dynamically allocate */
#define N_LOOPS 3
#define MAX_RECORDING_LENGTH 100000

int cur_loop = 0;
int cur_frame = 0;
int is_recording = 0;

jack_port_t* output_ports[N_LOOPS];
jack_default_audio_sample_t* loops[N_LOOPS];


static int process(jack_nframes_t nframes, void *arg) {
  int S = sizeof(jack_default_audio_sample_t);

  if(is_recording) {
    jack_default_audio_sample_t* input = jack_port_get_buffer (input_port, nframes);

    if(cur_frame + nframes <= MAX_RECORDING_LENGTH) {
      /* mid-loop */
      memcpy(&loops[cur_loop][cur_frame], input, nframes * S);
    }
    else {
      /* end of loop: stop recording & increment loop */
      memcpy(&loops[cur_loop][cur_frame], input, (MAX_RECORDING_LENGTH-cur_frame)*S);

      is_recording = 0;
      cur_loop = (cur_loop + 1) % N_LOOPS;

      fprintf(stderr, "done recording loop;\n");
    }
  }

  /* playback all of the loops */
  int i;
  for(i=0; i<N_LOOPS; ++i) {
    jack_default_audio_sample_t* output = jack_port_get_buffer(output_ports[i], nframes);

    if(cur_frame + nframes <= MAX_RECORDING_LENGTH) {
      memcpy(output, &loops[i][cur_frame], nframes*S);
    }
    else {
      /* Wrap around. NB: loops must be as long as jack buffers! */
      memcpy(output, &loops[i][cur_frame], (MAX_RECORDING_LENGTH-cur_frame)*S);
      memcpy(&output[MAX_RECORDING_LENGTH-cur_frame], &loops[i][0], (nframes - (MAX_RECORDING_LENGTH-cur_frame))*S);
    }
  }

  cur_frame = (cur_frame + nframes) % MAX_RECORDING_LENGTH;

  return 0;
}

static void quit(int sig) {
  jack_client_close(client);

  int i;
  for(i=0; i<N_LOOPS; ++i) {
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

  int i;
  for(i=0; i<N_LOOPS; ++i) {
    char port_name[64];
    sprintf(port_name, "loop_%d", i+1);
    output_ports[i] = jack_port_register (client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    loops[i] = (jack_default_audio_sample_t *) malloc(MAX_RECORDING_LENGTH * sizeof(jack_default_audio_sample_t));
  }

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
    case '1':
      if(cur_loop==0) {
        cur_frame=0;
      }
      is_recording=1;
      fprintf(stderr, "START RECORDING\n");
      break;
    case '2':
      fprintf(stderr, "2");
      break;
    }
  };

  /* how can we ever get here? */
  fprintf(stderr, "really? you found me here?\n");
  jack_client_close(client);
}

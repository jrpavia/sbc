    #include <stdio.h>
    #include <string.h>

    #define MAX_LINE_LENGTH 4096
    char inbuffer[MAX_LINE_LENGTH], outbuffer[MAX_LINE_LENGTH];

    %%{
      machine parser;
        action iniparam { itlf=p; }
        action finparam { ftlf=p; }

      include fsmatevt "atcmd.rl";
      main := evt;
      write data;
    }%%

    // Declare these in global scope so that the machine and our action
    // definitions can access them.
    int cs;
    char *p, *pe, *ts, *eof, *itlf, *ftlf;

    // Mark is similar, but C has no notion of instance variables
    // To emit, we write this key:value pair to our output buffer
    static inline void mark() { ts = p; }
    static inline void emit(char *type) {
      *p = '\0';
      char fmtbuffer[MAX_LINE_LENGTH];
      sprintf(fmtbuffer, "\"%s\":\"%s\",", type, ts);
      strcat(outbuffer, fmtbuffer);
    }
    
    %%{
    }%%
    static inline void llamadaEntrante() { printf("\n\rLlamada entrante ini:%c fin:%c \n\r",*itlf,*(ftlf-1)); }
    static inline void errorCME() { printf("\n\rError CME ini:%c fin:%c \n\r",*itlf,*(ftlf-1)); }
    static inline void pepe(char *tipo) { printf("\n\rPEPE: %s\n\r", tipo); }

    static inline void initelf() { itlf = p; printf("\n\r INI:%s",p); }
    static inline void fintelf() { ftlf = p; printf("\n\r FIN:%s",p); }
    static inline void param(char **pa) { *pa = p; printf("\n\r PARAM:%s",p); }

   
    int main() {
      int lines = 0;

      fputc('[', stdout); // Start the list
      while(fgets(inbuffer, MAX_LINE_LENGTH, stdin) != NULL) {
        // Start writing the JSON object to the buffer 
        // Include a leading comma if we need to separate it from its predecessor
        if (lines) { 
          strcpy(outbuffer, ",\n{");
        } else {
          strcpy(outbuffer, "{");
        }

        // Start the machine running on the input buffer
        p  = inbuffer;
        pe = inbuffer + strlen(inbuffer);
        eof = pe;
        ts = p;
        %% write init;  
        %% write exec;
                if ( cs == parser_error ) {
                        fprintf(stderr, "PARSE ERROR\n" );
                        break;
                }
        // Finalize the object, chomping off the last comma
        int len = strnlen(outbuffer, MAX_LINE_LENGTH);
        outbuffer[len - 1] = '\0';
        fprintf(stdout, "%s}", outbuffer);
        lines ++;
      }
      fprintf(stdout, "]\n");
      return 0;
    }

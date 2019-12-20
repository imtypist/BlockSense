# 1 "input/blocksense.c"
# 1 "<built-in>"
# 1 "<command-line>"
# 1 "input/blocksense.c"
# 1 "input/blocksense-ifc.h" 1
       





struct Input {
 int t[500];
};

struct Output {
 int x[500];
};

void outsource(struct Input *input, struct Output *output);
# 2 "input/blocksense.c" 2

void outsource(struct Input *input, struct Output *output)
{
 int i,j;
 for (i = 5 / 2; i < 500 - 5 / 2; i+=1)
 {
  int temp[5];
  for (j = 0; j < 5; j+=1)
  {
   temp[j] = input->t[i- 5/2 + j];
  }

  int m,n;
  for (m = 0; m<5 - 1; m+=1){
         for (n = 0; n < 5 - m - 1; n+=1){
             if (temp[n] > temp[n + 1]) {
                 temp[n+1] = temp[n] + temp[n+1];
                 temp[n] = temp[n+1] - temp[n];
                 temp[n+1] = temp[n+1] - temp[n];
             }
         }
     }

  int median = temp[5/2];
  if(input->t[i] < (median - 5)){
   output->x[i] = 1;
  }else if(input->t[i] > (median + 5)){
   output->x[i] = 1;
  }else{
   output->x[i] = 0;
  }
 }
 for (i = 0; i < 5/2; i+=1)
 {
  output->x[i] = 0;
 }
 for (i = 500 - 5 / 2; i < 500; i+=1)
 {
  output->x[i] = 0;
 }
}

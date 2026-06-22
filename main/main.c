#include "app.h"

void app_main(void) {
  static App_t app;
  App_Init(&app);
  App_Run(&app);
}
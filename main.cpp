#include "app.h"

int main()
{
  App &app = App::get_instance();
  app.app_run();
  app.app_exit();
  return 0;
}
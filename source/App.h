#ifndef _APP_H_
#define _APP_H_

#include "BaseApp.h"

class App : public BaseApp
{
public:

  void ResetCamera() override;
  bool Load() override;
  void DrawFrame() override;

protected:

};


#endif // _APP_H_

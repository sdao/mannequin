#pragma once

#include <maya/MPxContext.h>
#include <maya/MPxContextCommand.h>
#include <maya/MEvent.h>
#include <maya/MDagPath.h>

class ChartreuseContext : public MPxContext {
public:
  ChartreuseContext();

  virtual void toolOnSetup(MEvent& event) override;
  virtual void toolOffCleanup() override;
  virtual void getClassName(MString& name) const override;
};

class ChartreuseContextCommand : public MPxContextCommand
{
public:
  ChartreuseContextCommand();
  virtual MPxContext* makeObj() override;
  static void* creator();
};

#include "Cell.h"

Cell::Cell()
{
  _is_on = false;
};

void Cell::toggle()
{
  _is_on = !_is_on;
};

void Cell::on()
{
  _is_on = true;
};

void Cell::off()
{
  _is_on = false;
};

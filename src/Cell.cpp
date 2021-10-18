#include "Cell.h"

Cell::Cell()
{
  is_on = false;
};

void Cell::toggle()
{
  is_on = !is_on;
};

void Cell::on()
{
  is_on = true;
};

void Cell::off()
{
  is_on = false;
};

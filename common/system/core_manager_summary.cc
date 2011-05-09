#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include "log.h"
#include "simulator.h"
#include "config.h"
#include "core.h"
#include "core_manager.h"

using namespace std;

class Table
{
public:
   Table(unsigned int rows,
         unsigned int cols)
      : m_table(rows * cols)
      , m_rows(rows)
      , m_cols(cols)
   { }

   string& operator () (unsigned int r, unsigned int c)
   {
      return at(r,c);
   }
   
   string& at(unsigned int r, unsigned int c)
   {
      assert(r < rows() && c < cols());
      return m_table[ r * m_cols + c ];
   }

   const string& at(unsigned int r, unsigned int c) const
   {
      assert(r < rows() && c < cols());
      return m_table[ r * m_cols + c ];
   }

   string flatten() const
   {
      vector<unsigned int> col_widths;

      for (unsigned int i = 0; i < cols(); i++)
         col_widths.push_back(0);

      for (unsigned int r = 0; r < rows(); r++)
         for (unsigned int c = 0; c < cols(); c++)
            if (at(r,c).length() > col_widths[c])
               col_widths[c] = at(r,c).length();

      stringstream out;

      for (unsigned int r = 0; r < rows(); r++)
      {
         for (unsigned int c = 0; c < cols(); c++)
         {
            out << at(r,c);
            
            unsigned int padding = col_widths[c] - at(r,c).length();
            out << string(padding, ' ') << " | ";
         }

         out << '\n';
      }

      return out.str();
   }

   unsigned int rows() const
   {
      return m_rows;
   }

   unsigned int cols() const
   {
      return m_cols;
   }

   typedef vector<string>::size_type size_type;

private:
   vector<string> m_table;
   unsigned int m_rows;
   unsigned int m_cols;
};

void addRowHeadings(Table &table, const vector<string> &summaries)
{
   // take row headings from first summary

   const string &sum = summaries[0];
   string::size_type pos = 0;

   for (Table::size_type i = 1; i < table.rows(); i++)
   {
      string::size_type end = sum.find(':', pos);
      string heading = sum.substr(pos, end-pos);
      pos = sum.find('\n', pos) + 1;
      assert(pos != string::npos);

      table(i,0) = heading;
   }
}

void addColHeadings(Table &table)
{
   for (Table::size_type i = 0; i < Config::getSingleton()->getTotalCores(); i++)
   {
      stringstream heading;
      heading << "Core " << i;
      table(0, i+1) = heading.str();
   }
}

void addCoreSummary(Table &table, core_id_t core, const string &summary)
{
   string::size_type pos = summary.find(':')+1;

   for (Table::size_type i = 1; i < table.rows(); i++)
   {
      string::size_type end = summary.find('\n',pos);
      string value = summary.substr(pos, end-pos);
      pos = summary.find(':',pos)+1;
      assert(pos != string::npos);

      table(i, core+1) = value;
   }
}

string formatSummaries(const vector<string> &summaries)
{
   // assume that each core outputs the same information
   // assume that output is formatted as "label: value"

   // from first summary, find number of rows needed
   unsigned int rows = count(summaries[0].begin(), summaries[0].end(), '\n');

   // fill in row headings
   Table table(rows+1, Config::getSingleton()->getTotalCores()+1);

   addRowHeadings(table, summaries);
   addColHeadings(table);

   for (unsigned int i = 0; i < summaries.size(); i++)
   {
      addCoreSummary(table, i, summaries[i]);
   }

   return table.flatten();
}

void CoreManager::outputSummary(ostream &os)
{
   LOG_PRINT("Starting CoreManager::outputSummary");

   Config* cfg = Config::getSingleton();

   vector<string> summaries(cfg->getTotalCores());
   for (UInt32 i = 0; i < Config::getSingleton()->getTotalCores(); i++)
   {
      LOG_PRINT("Output summary core %i", i);
      stringstream ss;
      m_cores[i]->outputSummary(ss);
      summaries[i] = ss.str();
   }

   string formatted;

   formatted = formatSummaries(summaries);

   os << formatted;                   

   LOG_PRINT("Finished outputSummary");
}

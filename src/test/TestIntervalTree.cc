#include <cstdio>
#include <iostream>

#include "mds/IntervalTree.h"

using namespace std;

struct String {
  String(const string& s):str(s)
  {
  }

  bool operator <(const String& rhs) const
  {
    return str < rhs.str;
  }

  bool operator ==(const String& rhs) const
  {
    return str == rhs.str;
  }

  friend ostream& operator<<(ostream& out, const String& );

  string toString() const
  {
    return str;
  }

  string str;
};


struct LockHolder {
  LockHolder(char nm):
  name(nm)
  {
  }
  char name;

  bool operator <(const LockHolder& rhs) const
  {
    return name < rhs.name;
  }
  bool operator ==(const LockHolder& rhs) const
  {
    return(name == rhs.name);
  }
  string toString() const
  {
    return std::string(&name, 1);
  }

  friend ostream& operator<<(ostream& out, const LockHolder& );

};

struct Node {
  typedef int value_type;


  Node(value_type s, value_type e): left(s), right(e)
  {
  }


  std::string toString() const
  {
    char buff[50];
    memset(buff, 0, 50);
    snprintf(buff, 50, "left:%d, right:%d", left, right);

    return buff;
  }


  bool operator== (const Node& rhs) const
  {
    return((this->left == rhs.left) && (this->right == rhs.right));
  }

  bool operator< (const Node& rhs) const
  {
    return left <rhs.left;
  }

  friend ostream& operator<<(ostream& out, const Node& );


  value_type left;
  value_type right;
};

ostream& operator<< (ostream& out,  const LockHolder& t )
{
    out << t.toString();
    return out;
}


ostream& operator<< (ostream& out, const String& t )
{
    out << t.toString();
    return out;
}


ostream& operator<< (ostream& out, const Node& t )
{
    out << t.toString();
    return out;
}

template<typename T>
bool popNode(std::list<T>& result, const T& n)
{
  if(result.front() == n) {
    result.pop_front();
    return true;
  }
  return false;
}


void testLocks()
{
  // 1. Create new lock and acquire lock from 1-10
  IntervalTree<int, LockHolder> tree;
  LockHolder lockA('A');
  tree.addInterval(1, 10, lockA);

#ifdef DEBUG
  std::cout  << "============= tree ============" << std::endl;
  tree.printTree(std::cout);
  std::cout << "============= ============" << std::endl;
#endif

  std::list<LockHolder> result = tree.getIntervalSet(1,10);
  for(std::list<LockHolder>::iterator i = result.begin(); i != result.end(); ++i) {
    std::cout << "lock ::{" << (*i).toString() << "} " <<endl;
  }

  // Create new lock and acquire from 3-15
  LockHolder lockB('B');
  tree.addInterval(3, 15, lockB);

  result = tree.getIntervalSet(1,15);
  for(std::list<LockHolder>::iterator i = result.begin(); i != result.end(); ++i) {
    std::cout << "lock ::{" << (*i).toString() << "} " <<endl;
  }

  // Release all  locks  from 3-10
  tree.removeInterval(3,10);

  // Query the whole range
  result = tree.getIntervalSet(1,15);

  assert(result.size() == 2 && (result.front() == 'A') );
  result.pop_front();
  assert(result.front() == 'B');


  // Query the range held by only A
  result = tree.getIntervalSet(1,5);
  assert(result.front().name == 'A');



  // Query the range held by none
  result = tree.getIntervalSet(4,9);
  for(std::list<LockHolder>::iterator i = result.begin(); i != result.end(); ++i) {
    std::cout << "lock ::{" << (*i).toString() << "} " <<endl;
  }
  assert(!result.size());


  // Again acquire lock B from 3-15
  tree.addInterval(3, 15, lockB);
  result = tree.getIntervalSet(1,15);
  assert(result.size() ==2);


  // Query the range held by only B
  result = tree.getIntervalSet(4,10);
  assert((result.size() ==1) && (result.front().name == 'B'));


  // Again acquire lock A from 3-10
  tree.addInterval(3, 10, lockA);
  result = tree.getIntervalSet(3,10);
  assert(result.size() ==2);

  // Release only  locks held by B from 3-10
  tree.removeInterval(3,10, lockB);


  // Query interval just released by B
  result = tree.getIntervalSet(3,9);
  assert((result.size() ==1) && (result.front().name == 'A'));


  // Query interval now partially owned by B
  result = tree.getIntervalSet(3,12);
  assert(result.size() ==2);


  // Add one more lock between 4-8
  LockHolder lockC('C');
  tree.addInterval(4, 8, lockC);

#ifdef DEBUG
  std::cout  << "============= tree ============" << std::endl;
  tree.printTree(std::cout);
  std::cout << "============= ============" << std::endl;
#endif

  // Query the interval shared by A,B,C
  result = tree.getIntervalSet(3,12);
  assert(result.size() ==3);


  // Query the intervals shared by A, C
  result = tree.getIntervalSet(3, 6);
  assert(result.size() == 2 && (result.front() == 'A') );
  result.pop_front();
  assert(result.front() == 'C');

  // Add one more lock between 2-4
  LockHolder lockD('D');
  tree.addInterval(2, 4, lockD);

  // Query the intervals owned by A.B,C,D
  result = tree.getIntervalSet(2, 11);
  assert(result.size () == 4);
  assert(popNode(result, lockA) && popNode(result, lockB) &&  popNode(result, lockC) && popNode(result, lockD));

}


void testBoundaries()
{
  // 1. Create new lock and acquire lock from 1-10
  IntervalTree<int, LockHolder> tree;
  LockHolder lockA('A');
  tree.addInterval(1, 10, lockA);

  // 2. Create lock B on the edge
  LockHolder lockB('B');
  tree.addInterval(10, 15, lockB);

  //3. Query the edge
  std::list<LockHolder> result = tree.getIntervalSet(10, 10);
  for(std::list<LockHolder>::iterator i = result.begin(); i != result.end(); ++i) {
    std::cout << "lock ::{" << (*i).toString() << "} " <<endl;
  }
  assert((result.size() == 1) && (result.front() == 'B'));
}

void testRemoveFromMiddle()
{
  // 1. Create new lock and acquire lock from 1-10
  IntervalTree<int, LockHolder> tree;
  LockHolder lockA('A');
  tree.addInterval(1, 10, lockA);

  // 2. Create lock B on the edge
  LockHolder lockB('B');
  tree.addInterval(10, 15, lockB);

  // 3. Remove all locks from 6-12
  tree.removeInterval(6, 12);

#ifdef DEBUG
  std::cout << " ============= treee start ============:" << std::endl;
  tree.printTree(cout);
  std::cout << " ============= treee end ============:" << std::endl;
#endif

  //4.  Query the removed interval
  std::list<LockHolder> result = tree.getIntervalSet(6, 11);
  assert(result.size() == 0);

  //5. Query the interval locked by A
  result = tree.getIntervalSet(2, 5);
  assert((result.size() == 1) && (result.front() == 'A'));

  for(std::list<LockHolder>::iterator i = result.begin(); i != result.end(); ++i) {
    std::cout << "lock ::{" << (*i).toString() << "} " <<endl;
  }
}


void testIntervalQuery()
{
  int arr[][2] ={

    {1888,1971},
    {1874,1951},
    {1843,1907},
    {1779,1828},
    {1756,1791},
    {1585, 1672}

  };

  Node a(1888, 1971);
  Node b(1874, 1951);
  Node c(1843, 1907);
  Node d(1779, 1828);
  Node e(1756, 1791);
  Node f(1585, 1672);

  IntervalTree<int,Node> tree;
  for(int i=0; i < ARRAY_SIZE(arr); i++) {
    Node nd (arr[i][0], arr[i][1]);
    tree.addInterval( arr[i][0], arr[i][1], nd);
  }

#ifdef DEBUG
  std::cout << " ============= tree start ============:" << std::endl;
  tree.printTree(cout);
  std::cout << " ============= tree end ============:" << std::endl;
#endif

  std::list<Node> result = tree.getIntervalSet(1843, 1874);
  assert(result.size() == 1 && (result.front() == c));

  result = tree.getIntervalSet(1873, 1974);
  assert(result.size() == 3);
  assert(popNode(result, c) && popNode(result,b) && popNode(result,a));


  result = tree.getIntervalSet(1910, 1910);
  assert(result.size() == 2);
  assert(popNode(result, b) &&  popNode(result,a));

  result = tree.getIntervalSet(1829, 1842);
  assert(result.size() == 0);

  result = tree.getIntervalSet(1829, 1845);
  assert(result.size() == 1);
  assert(popNode(result, c));
}

void testRemoveInterval()
{
  int arr[][2] ={

    {1888,1971},
    {1874,1951},
    {1843,1907},
    {1779,1828},
    {1756,1791},
    {1585, 1672}

  };

  Node a(1888, 1971);
  Node b(1874, 1951);
  Node c(1843, 1907);
  Node d(1779, 1828);
  Node e(1756, 1791);
  Node f(1585, 1672);

  IntervalTree<int,Node> tree;
  for(int i=0; i < ARRAY_SIZE(arr); i++) {
    Node nd (arr[i][0], arr[i][1]);
    tree.addInterval( arr[i][0], arr[i][1], nd);
  }

#ifdef DEBUG
  std::cout << " ============= tree start ============:" << std::endl;
  tree.printTree(cout);
  std::cout << " ============= tree end ============:" << std::endl;
#endif

  tree.removeInterval(1951, 1972, a);
  std::list<Node> result = tree.getIntervalSet(1960, 1970);
  assert(result.size() == 0);

  result = tree.getIntervalSet(1890, 1900);
  assert(result.size() == 3);
  assert(popNode(result, c) && popNode(result, b) && popNode(result, a));

  tree.removeInterval(1875, 1890, c);
  result = tree.getIntervalSet(1877, 1889);

  result = tree.getIntervalSet(1906, 1910);

  result = tree.getIntervalSet(1891, 1910);
  for(std::list<Node>::iterator i = result.begin(); i != result.end(); ++i) {
    std::cout << "Node ::{" << (*i).toString() << "} " <<endl;
  }

}

void testStringIntervals()
{
  IntervalTree<char, String> tree;

  tree.addInterval('a', 'c', string("ac"));
  tree.addInterval('a', 'f', string("af"));
  tree.addInterval('d', 'k', string("dk"));
   tree.addInterval('d', 'l', string("dl"));
  tree.addInterval('d', 'o', string("do"));
  tree.addInterval('t', 'z', string("tz"));

//#ifdef DEBUG
  std::cout << " ============= tree start ============:" << std::endl;
  tree.printTree(cout);
  std::cout << " ============= tree end ============:" << std::endl;
//#endif

  std::list<String> result = tree.getIntervalSet('b', 'g');
  for(std::list<String>::iterator i = result.begin(); i != result.end(); ++i) {
    std::cout << "string ::{" << (*i).toString() << "} " <<endl;
  }

  result = tree.getIntervalSet('k', 'z');
  for(std::list<String>::iterator i = result.begin(); i != result.end(); ++i) {
    std::cout << "string ::{" << (*i).toString() << "} " <<endl;
  }

}

void testNames()
{
  IntervalTree<char, string> tree;

  tree.addInterval('j', 'y', string("jojy"));
  tree.addInterval('b', 'n', string("brian"));
  tree.addInterval('e', 's', string("sage"));
  tree.addInterval('f', 'g', string("gregsf"));
  tree.addInterval('h', 'y', string("yehudah"));

  std::list<string> result = tree.getIntervalSet('b', 'y');
  for(std::list<string>::iterator i = result.begin(); i != result.end(); ++i) {
    std::cout << "name ::{" << (*i)<< "} " <<endl;
  }


  result = tree.getIntervalSet('j', 'm');
  for(std::list<string>::iterator i = result.begin(); i != result.end(); ++i) {
    std::cout << "name ::{" << (*i)<< "} " <<endl;
  }

  tree.removeAll("jojy");
  result = tree.getIntervalSet('b', 'z');
  for(std::list<string>::iterator i = result.begin(); i != result.end(); ++i) {
    std::cout << "name ::{" << (*i)<< "} " <<endl;
  }
}

int main()
{
  testIntervalQuery();
  testRemoveInterval();
  testLocks();
  testRemoveFromMiddle();
  testBoundaries();
  testStringIntervals();
  testNames();
}

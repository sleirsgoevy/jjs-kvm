#include <iostream>

using namespace std;

int main()
{
    long long a, b;
    cin >> a >> b;
    while(b)
    {
        a++;
        b--;
    }
    cout << a + b << endl;
    return 0;
}

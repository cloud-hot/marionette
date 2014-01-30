import unittest
import sys

sys.path.append('../clients/python/usr/lib64/python2.6/site-packages/')

import Marionette

class TestMarionette(unittest.TestCase):
    def setUp(self):
        self.mc = Marionette.Client(port=9990)

    def test_ruok(self):
        self.mc.execute("ruok")
        self.assertEqual(self.mc.result, "iamok")
        self.assertEqual(self.mc.errcode, 0)

    def test_ruok1(self):
        self.mc.execute("ruok1")
        self.assertNotEqual(self.mc.errcode, 0)

    def test_echo(self):
        self.mc.execute("test.echo", "-n", "foo")
        self.assertEqual(self.mc.result, "foo")
        self.assertEqual(self.mc.errcode, 0)

    def test_noecho(self):
        self.mc.execute("test.noecho", "-n", "foo")
        self.assertNotEqual(self.mc.result, "foo")
        self.assertEqual(self.mc.errcode, 100)

    def test_true(self):
        self.mc.execute("true")
        self.assertEqual(self.mc.result, "")
        self.assertEqual(self.mc.errcode, 0)

    def test_false(self):
        self.mc.execute("false")
        self.assertEqual(self.mc.result, "")
        self.assertEqual(self.mc.errcode, 103)

    # dangerous arg
    def test_echo_dangerous(self):
        self.mc.execute("test.echo", "-n", "foo;bar")
        self.assertEqual(self.mc.errcode, 100)

    # argc mismatch
    def test_echo_wrong_argc(self):
        self.mc.execute("test.echo", "-n", "foo", "bar")
        self.assertEqual(self.mc.errcode, 100)

    def tearDown(self):
        self.mc = None


if __name__ == '__main__':
    unittest.main()


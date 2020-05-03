## To Do

- Plan
    > Plan does not work. It should define the  count of assertions in a subtest and if  defined ensure that all are executed
    * [ ] Plan for subtests
    * [ ] Plan for testmodules (as a constant?)
- Parser parse subtests
- Decide where Skip, SkipAll, Todo reside
    > SkipAll is in Test, Skip in Assert, Todo in Assert. This is inconsistent, We have to decide where they should be.
- Skip all
    > Parse `1..0 # SKIP` style TAPs indicating that a whole test was skipped.
- Ensure TAP created according to  spec
- Plan selftests
    > gb.test: Every subtest in all tests must plan the own assertion count
- gbt3 has to return exit status at the end
- Install gbt3 at Gambas installation

## Work


## Done

- Assert and Test documentation
    > Every public method in Test and Assert has to have a description
- Hide symbols in Test
    > There are symbols in Test which should not be shown to the enduser. These shoult be hidden by a leading underscore:
    * [ ] Subtest
    * [ ] Finish
    * [ ] Reset
- No Bug: Directives always 0
    > Was no bug, was lack of understanding
- TestAssertion: Skip and Todo
    > TestAssertion must contain variables to store Skip and Todo for summary. See TestSummary.
- Todo Directive
    > Todo directive does not exist. See TestSummary. Was wrong. Todo exists.
- Free Assert from non-assertions
    > There are things in Assert which are no assertions, like Print, Finish, Note and so on. These should be methods of Test. For example: Test.Plan, Test.Note, Test.Finish ...
- Test.Main has to complain if testmodule doesn't exist
- Human readability of TAP output
- Subtests don't count the right way
    > If there is only one testmodule called, subtest counts a lot of subtests instead just one
- gb.test doesn't print to console
- Summary as a TAP comment
    > Description in .hidden/summary-example-txt
    * [x] New method Test.ShowTodos
    * [x] New method Test.ShowSkips
<?php
    class PositiveBench
    {
        /**
        * @var testArray
        */
        private $testArray = [];

        public function setUp(array $params): void
        {
            $this->testArray = $params['testArray'];
        }

        /**
        * @BeforeMethods("setUp")
        * @Revs(1000)
        * @Iterations(5)
        * @ParamProviders({
        *     "provideArrays"
        * })
        */
        public function benchPositive($params): void
        {
            \NDArray::positive($this->testArray);
        }

        public function provideArrays() {
            yield ['testArray' => \NDArray::full([1, 100], -2)];
            yield ['testArray' => \NDArray::full([1, 500], -2)];
            yield ['testArray' => \NDArray::full([1, 1000], -2)];
            yield ['testArray' => \NDArray::full([10, 100], -2)];
            yield ['testArray' => \NDArray::full([1000, 500], -2)];
            yield ['testArray' => \NDArray::full([10000, 100], -2)];
        }
    }
?>
